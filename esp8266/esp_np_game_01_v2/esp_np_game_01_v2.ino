// V2: 長按2秒 --> reset game

#include <Adafruit_NeoPixel.h>

// ESP8266 Pin Map Note:
// D0-GPIO16 | D1-GPIO5 | D2-GPIO4 | D3-GPIO0 | D4-GPIO2
// D5-GPIO14 | D6-GPIO12 | D7-GPIO13 | D8-GPIO15
// -----------------------------------------------------------------------------
// 參數設定
// -----------------------------------------------------------------------------
// !!! 根據您的要求，使用 D2 (對應 ESP8266 的 GPIO 4) 控制燈條
#define PIN D2       // Neopixel 數據腳位 (D2, 實際 GPIO 4)

// 為了避開 D4 (GPIO 2, 啟動時為 HIGH)，改用 D1 (GPIO 5) 或其他腳位
#define BUTTON_PIN D1       // 子彈按鈕腳位 (D1, 實際 GPIO 5)

// 使用 D7 (實際 GPIO 13) 作為蜂鳴器腳位
#define BUZZER_PIN D7       // Buzzer 腳位 (D7, 實際 GPIO 13)

#define NUMPIXELS 20       // 燈條上的燈珠數量
#define BULLET_SPEED 20       // 子彈每移動一格的延遲時間 (毫秒)
#define START_INTERVAL 1000     // 初始燈光移動/生成的時間間隔 (毫秒)
#define MIN_INTERVAL 200       // 燈光移動/生成的最快時間間隔 (毫秒)
#define SPEED_INCREMENT 50       // 每次難度提升減少的毫秒數
#define KILLS_PER_LEVEL 5        // 每擊殺多少敵人提升一次難度

// --- 新增: 長按重新開始設定 ---
#define RESTART_HOLD_TIME 2000   // 重新開始所需的長按時間 (毫秒)

// 定義顏色
const uint32_t GAMEOVER_COLOR = Adafruit_NeoPixel::Color(20, 0, 0); 
const uint32_t COLOR_HP1      = Adafruit_NeoPixel::Color(0, 20, 0);   
const uint32_t COLOR_HP2      = Adafruit_NeoPixel::Color(0, 0, 20);   
const uint32_t COLOR_HP3      = Adafruit_NeoPixel::Color(20, 20, 20); 
const uint32_t BULLET_COLOR   = Adafruit_NeoPixel::Color(30, 30, 0); 

// -----------------------------------------------------------------------------
// 狀態變數
// -----------------------------------------------------------------------------
// NeoPixel 實例化，注意 ESP8266 的 NeoPixel 函式庫已經處理了 GPIO 映射
Adafruit_NeoPixel pixels(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);

// 遊戲狀態
int pixelHP[NUMPIXELS] = {0}; 
unsigned long lastMoveTime = 0; 
bool isGameOver = false;        
int buttonState = 0;            
int lastButtonState = 0;        

// --- 新增: 重新開始追蹤變數 ---
unsigned long buttonPressStartTime = 0;  // 記錄按鈕按下的時間點
bool isButtonHeld = false;               // 追蹤按鈕是否正在被按住

// 子彈狀態
int bulletPos = -1;             
int targetIndex = -1;           
unsigned long lastBulletMoveTime = 0;

// 難度追蹤
int killCount = 0;             // 總擊殺數
int currentInterval = START_INTERVAL; // 當前燈光移動/生成間隔
int currentLevel = 0;           // 當前難度等級 (每 5 殺升級)


// -----------------------------------------------------------------------------
// Function Prototypes (為了讓編譯器知道函數存在)
// -----------------------------------------------------------------------------
void updateDifficulty();
void handleLightMovement();
void handleShooting();
void handleBulletMovement();
void drawLights();
void gameOverSequence();
void resetGame();
void handleRestart();


// -----------------------------------------------------------------------------
// Buzzer 音效函數 (使用 ESP8266 的 PWM 模擬 tone)
// -----------------------------------------------------------------------------
void playShootSound() {
  // ESP8266 不支援標準 tone()，我們使用 analogWrite 模擬 PWM
  
  // 設置頻率 (1000 Hz)
  analogWriteFreq(1000); 
  // 設置 PWM 佔空比 (約 50% 響度)
  analogWrite(BUZZER_PIN, 512); 
  
  // 持續 50 毫秒
  delay(50);
  
  // 關閉聲音 (設置為 0)
  analogWrite(BUZZER_PIN, 0); 
}


// -----------------------------------------------------------------------------
// setup() 初始設定
// -----------------------------------------------------------------------------
void setup() {
  Serial.begin(115200); // ESP8266 通常使用 115200 鮑率
  randomSeed(analogRead(A0)); // A0 是 ADC 輸入腳位，ESP8266 只有一個

  // 設定按鈕為 INPUT_PULLUP
  pinMode(BUTTON_PIN, INPUT_PULLUP);  
  
  // 設定 Buzzer 腳位為輸出
  pinMode(BUZZER_PIN, OUTPUT); 
  // 初始化 PWM 頻率，確保 analogWrite 可以工作
  analogWriteRange(1023); // 預設值，確保佔空比範圍是 0-1023

  pixels.begin();
  pixels.show();  
  
  Serial.println("\nGame Initialized on ESP8266. Start Interval: 1000ms");
}

// -----------------------------------------------------------------------------
// loop() 迴圈執行 (修改: 遊戲結束時呼叫 handleRestart)
// -----------------------------------------------------------------------------
void loop() {
  if (isGameOver) {
    gameOverSequence();
    handleRestart(); // 在遊戲結束時檢查長按重新開始
    delay(10); 
    return;
  }

  // 1. 處理燈光移動和生成 (使用動態間隔 currentInterval)
  if (millis() - lastMoveTime >= currentInterval) {
    handleLightMovement();
    lastMoveTime = millis();
  }

  // 2. 處理子彈射擊
  handleShooting();

  // 3. 處理子彈的移動
  handleBulletMovement();

  // 4. 繪製燈條
  drawLights();
}

// -----------------------------------------------------------------------------
// 難度計算函數 (保持不變)
// -----------------------------------------------------------------------------
void updateDifficulty() {
    // 計算新的難度等級 (每 KILLS_PER_LEVEL 擊殺升級)
    int newLevel = killCount / KILLS_PER_LEVEL;

    // 如果難度等級提升了
    if (newLevel > currentLevel) {
        currentLevel = newLevel;
        
        // 計算新的間隔時間
        int timeReduction = currentLevel * SPEED_INCREMENT;
        currentInterval = START_INTERVAL - timeReduction;
        
        // 確保間隔不會低於最小值
        if (currentInterval < MIN_INTERVAL) {
            currentInterval = MIN_INTERVAL;
        }

        Serial.print("LEVEL UP! Kill Count: ");
        Serial.print(killCount);
        Serial.print(", New Interval: ");
        Serial.print(currentInterval);
        Serial.println("ms");
    }
}

// -----------------------------------------------------------------------------
// 處理燈光移動和生成 (保持不變)
// -----------------------------------------------------------------------------
void handleLightMovement() {
  
  // A. 檢查遊戲結束條件 (Index 0 的燈是否亮著)
  if (pixelHP[0] > 0) {
    Serial.println("Game Over: Light reached Index 0!");
    isGameOver = true;
    return;
  }
  
  // B. 燈光移動 (i -> i-1)
  for (int i = 1; i < NUMPIXELS; i++) {
    pixelHP[i - 1] = pixelHP[i];  
  }

  // C. 隨機生成新燈 (在 Index 19)
  int randomHP = random(4); 
  pixelHP[NUMPIXELS - 1] = randomHP;  
}


// -----------------------------------------------------------------------------
// 處理子彈射擊 (使用修改後的 playShootSound)
// -----------------------------------------------------------------------------
void handleShooting() {
  buttonState = digitalRead(BUTTON_PIN);

  if (bulletPos == -1 && buttonState == LOW && lastButtonState == HIGH) {
    
    int foundTarget = -1;
    for (int i = 0; i < NUMPIXELS; i++) {
      if (pixelHP[i] > 0) {
        foundTarget = i;
        break;  
      }
    }
    
    if (foundTarget != -1) {
      targetIndex = foundTarget;  
      bulletPos = 0;              
      lastBulletMoveTime = millis();
      
      // 播放射擊音效
      playShootSound(); 
    }
  }

  lastButtonState = buttonState;
}

// -----------------------------------------------------------------------------
// 處理子彈移動與命中 (保持不變)
// -----------------------------------------------------------------------------
void handleBulletMovement() {
  if (bulletPos == -1) return; 

  if (millis() - lastBulletMoveTime >= BULLET_SPEED) {
    
    bulletPos++;
    lastBulletMoveTime = millis();

    if (bulletPos >= targetIndex) {
      
      // 檢查目標在命中前是否已存活
      if (pixelHP[targetIndex] > 0) {
        
        pixelHP[targetIndex]--; 
        
        // 檢查是否完成擊殺 (生命值歸零)
        if (pixelHP[targetIndex] == 0) {
            killCount++; // 擊殺數 +1
            updateDifficulty(); // 檢查是否升級
            Serial.print("TARGET KILLED! Total Kills: ");
            Serial.println(killCount);
        } else {
            Serial.print("Target hit. New HP: ");
            Serial.println(pixelHP[targetIndex]);
        }
      }
      
      bulletPos = -1;
      targetIndex = -1;
    }
  }
}


// -----------------------------------------------------------------------------
// 繪製燈條 (保持不變)
// -----------------------------------------------------------------------------
void drawLights() {
  // 1. 繪製背景 (燈光目標)
  for (int i = 0; i < NUMPIXELS; i++) {
    uint32_t colorToSet = 0;  
    
    switch (pixelHP[i]) {
      case 3: colorToSet = COLOR_HP3; break;
      case 2: colorToSet = COLOR_HP2; break;
      case 1: colorToSet = COLOR_HP1; break;
      case 0: 
      default: colorToSet = 0; break;
    }

    pixels.setPixelColor(i, colorToSet);
  }
  
  // 2. 覆蓋繪製子彈
  if (bulletPos != -1 && bulletPos < NUMPIXELS) {
    pixels.setPixelColor(bulletPos, BULLET_COLOR);  
  }

  pixels.show();  
}

// -----------------------------------------------------------------------------
// 遊戲結束序列 (修改: 顯示 "重新開始" 提示)
// -----------------------------------------------------------------------------
void gameOverSequence() {
  // 閃爍 "GAMEOVER" 顏色
  for(int i=0; i < NUMPIXELS; i++) {
    pixels.setPixelColor(i, GAMEOVER_COLOR);
  }
  pixels.show();
  
  // 這裡可以加入提示，例如閃爍某個顏色來提示可以長按
  // 為了不干擾長按的偵測，這裡不再做延遲的閃爍動畫
}

// -----------------------------------------------------------------------------
// --- 新增函數: 重置遊戲狀態 ---
// -----------------------------------------------------------------------------
void resetGame() {
  Serial.println("--- Restarting Game ---");
  
  // 1. 重置所有燈珠 HP
  for(int i=0; i < NUMPIXELS; i++) {
    pixelHP[i] = 0;
  }
  pixels.clear(); 
  pixels.show(); 
  
  // 2. 重置遊戲狀態變數
  isGameOver = false;
  lastMoveTime = millis();
  
  // 3. 重置子彈狀態
  bulletPos = -1;             
  targetIndex = -1;           
  lastBulletMoveTime = 0;
  
  // 4. 重置難度追蹤
  killCount = 0;
  currentLevel = 0;
  currentInterval = START_INTERVAL;
  
  // 5. 重置按鈕狀態追蹤 (確保不會立即再次觸發)
  buttonPressStartTime = 0;
  isButtonHeld = false;
  lastButtonState = HIGH; // 假設按鈕初始為未按下 (INPUT_PULLUP)
  buttonState = HIGH; 
  
  Serial.print("Game Reset. Start Interval: ");
  Serial.print(currentInterval);
  Serial.println("ms");
}

// -----------------------------------------------------------------------------
// --- 新增函數: 處理長按重新開始 ---
// -----------------------------------------------------------------------------
void handleRestart() {
  // 讀取按鈕狀態
  buttonState = digitalRead(BUTTON_PIN);
  unsigned long currentTime = millis();

  // 1. 按鈕剛被按下 (LOW: 被按下，因為是 INPUT_PULLUP)
  if (buttonState == LOW && lastButtonState == HIGH) {
    buttonPressStartTime = currentTime;
    isButtonHeld = true;
    Serial.println("Button pressed. Start timer.");
  } 
  // 2. 按鈕被放開
  else if (buttonState == HIGH && lastButtonState == LOW) {
    // 重置計時器，無論長按是否足夠時間
    buttonPressStartTime = 0;
    isButtonHeld = false;
    Serial.println("Button released. Timer reset.");
  }
  
  // 3. 按鈕持續被按住，並達到長按時間
  if (isButtonHeld && (currentTime - buttonPressStartTime >= RESTART_HOLD_TIME)) {
    Serial.println("Long press detected! Restarting game...");
    resetGame();  // 執行遊戲重置
    // 確保放開前不會再次觸發重置
    isButtonHeld = false; 
    buttonPressStartTime = 0; 
  }
  
  // 更新上次按鈕狀態
  lastButtonState = buttonState;
}
