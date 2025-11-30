#include <Adafruit_NeoPixel.h>

// -----------------------------------------------------------------------------
// 📌 參數設定
// -----------------------------------------------------------------------------
#define PIN             2        // Neopixel 數據腳位 (D2)
#define BUTTON_PIN      4        // 子彈按鈕腳位 (D4)
#define NUMPIXELS       20       // 燈條上的燈珠數量
#define BULLET_SPEED    20       // 子彈每移動一格的延遲時間 (毫秒)
#define START_INTERVAL  1000     // 初始燈光移動/生成的時間間隔 (毫秒)
#define MIN_INTERVAL    200      // 燈光移動/生成的最快時間間隔 (毫秒)
#define SPEED_INCREMENT 50       // 每次難度提升減少的毫秒數
#define KILLS_PER_LEVEL 5        // 每擊殺多少敵人提升一次難度

// 定義顏色
const uint32_t GAMEOVER_COLOR = Adafruit_NeoPixel::Color(20, 0, 0); 
const uint32_t COLOR_HP1      = Adafruit_NeoPixel::Color(0, 20, 0);   
const uint32_t COLOR_HP2      = Adafruit_NeoPixel::Color(0, 0, 20);   
const uint32_t COLOR_HP3      = Adafruit_NeoPixel::Color(20, 20, 20); 
const uint32_t BULLET_COLOR   = Adafruit_NeoPixel::Color(30, 30, 0);  

// -----------------------------------------------------------------------------
// 💾 狀態變數
// -----------------------------------------------------------------------------
Adafruit_NeoPixel pixels(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);

// 遊戲狀態
int pixelHP[NUMPIXELS] = {0}; 
unsigned long lastMoveTime = 0; 
bool isGameOver = false;        
int buttonState = 0;           
int lastButtonState = 0;       

// 子彈狀態
int bulletPos = -1;             
int targetIndex = -1;           
unsigned long lastBulletMoveTime = 0;

// 難度追蹤 **NEW**
int killCount = 0;              // 總擊殺數
int currentInterval = START_INTERVAL; // 當前燈光移動/生成間隔
int currentLevel = 0;           // 當前難度等級 (每 5 殺升級)

// -----------------------------------------------------------------------------
// ⚙️ setup() 初始設定
// -----------------------------------------------------------------------------
void setup() {
  Serial.begin(9600);
  randomSeed(analogRead(A0));
  
  pinMode(BUTTON_PIN, INPUT_PULLUP); 

  pixels.begin();
  pixels.show(); 
  
  Serial.println("Game Initialized. Start Interval: 1000ms");
}

// -----------------------------------------------------------------------------
// 🔄 loop() 迴圈執行
// -----------------------------------------------------------------------------
void loop() {
  if (isGameOver) {
    gameOverSequence();
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
// 🔄 難度計算函數 **NEW**
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

        Serial.print("--- LEVEL UP! --- Kill Count: ");
        Serial.print(killCount);
        Serial.print(", New Interval: ");
        Serial.print(currentInterval);
        Serial.println("ms");
    }
}

// -----------------------------------------------------------------------------
// 🎯 處理燈光移動和生成 (使用 currentInterval)
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
// 🔫 處理子彈射擊 (與第四版相同，只負責發射)
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
    }
  }

  lastButtonState = buttonState;
}

// -----------------------------------------------------------------------------
// 💥 處理子彈移動與命中 (加入擊殺計數)
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
        
        // **NEW**：檢查是否完成擊殺 (生命值歸零)
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
// 🎨 繪製燈條 (與第四版相同)
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
// 🛑 遊戲結束序列 (與第三版相同)
// -----------------------------------------------------------------------------
void gameOverSequence() {
  for(int i=0; i < NUMPIXELS; i++) {
    pixels.setPixelColor(i, GAMEOVER_COLOR);
  }
  pixels.show();
  delay(100); 
}
