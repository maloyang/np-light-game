#include <Adafruit_NeoPixel.h>

// ESP8266 Pin Map Note:
// D0-GPIO16 | D1-GPIO5 | D2-GPIO4 | D3-GPIO0 | D4-GPIO2
// D5-GPIO14 | D6-GPIO12 | D7-GPIO13 | D8-GPIO15
// -----------------------------------------------------------------------------
// 參數設定
// -----------------------------------------------------------------------------
#define PIN D2              // Neopixel 數據腳位 (D2, 實際 GPIO 4)
#define BUTTON_PIN D1       // 標準子彈按鈕腳位 (D1, 實際 GPIO 5)
#define POWER_BUTTON_PIN D3 // 強力攻擊按鈕腳位 (D3, 實際 GPIO 0)
#define BUZZER_PIN D7       // Buzzer 腳位 (D7, 實際 GPIO 13)

#define NUMPIXELS 20        // 燈條上的燈珠數量
#define BULLET_SPEED 20     // 標準子彈每移動一格的延遲時間 (毫秒)
#define POWER_BULLET_SPEED 40 // 強力子彈每移動一格的延遲時間 (毫秒，速度較慢，特效更明顯) 

#define START_INTERVAL 1000 // 初始燈光移動/生成的時間間隔 (毫秒)
#define MIN_INTERVAL 200    // 燈光移動/生成的最快時間間隔 (毫秒)
#define SPEED_INCREMENT 50  // 每次難度提升減少的毫秒數
#define KILLS_PER_LEVEL 10   // 每擊殺多少敵人提升一次難度
#define RESTART_HOLD_TIME 2000 // 重新開始所需的長按時間 (毫秒)

// --- 強力攻擊參數 ---
#define KILLS_FOR_POWER_SHOT 10 // 擊殺數需求 (10 殺一發)
#define POWER_SHOT_KILL_COUNT 3 // 強力攻擊消滅的敵人數

// 定義顏色
const uint32_t GAMEOVER_COLOR = Adafruit_NeoPixel::Color(20, 0, 0);
const uint32_t COLOR_HP1      = Adafruit_NeoPixel::Color(0, 20, 0);
const uint32_t COLOR_HP2      = Adafruit_NeoPixel::Color(0, 0, 20);
const uint32_t COLOR_HP3      = Adafruit_NeoPixel::Color(20, 20, 20);
const uint32_t BULLET_COLOR   = Adafruit_NeoPixel::Color(30, 30, 0);

// 強力攻擊顏色 (頭部 - 最亮粉紅)
const uint32_t POWER_SHOT_COLOR  = Adafruit_NeoPixel::Color(30, 0, 30); 
// 尾巴顏色 1 (中等亮度)
const uint32_t POWER_TAIL1_COLOR = Adafruit_NeoPixel::Color(20, 0, 20); 
// 尾巴顏色 2 (最暗亮度)
const uint32_t POWER_TAIL2_COLOR = Adafruit_NeoPixel::Color(10, 0, 10); 
// 準備燈顏色
const uint32_t POWER_READY_COLOR = Adafruit_NeoPixel::Color(10, 0, 10); 

// -----------------------------------------------------------------------------
// 狀態變數
// -----------------------------------------------------------------------------
Adafruit_NeoPixel pixels(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);

// 遊戲狀態
int pixelHP[NUMPIXELS] = {0};
unsigned long lastMoveTime = 0;
bool isGameOver = false;

// 按鈕狀態追蹤
int buttonState = 0;
int lastButtonState = 0;

int powerButtonState = 0;
int lastPowerButtonState = 0;

// 長按重新開始追蹤變數
unsigned long buttonPressStartTime = 0; 
bool isButtonHeld = false;             

// 子彈狀態
int bulletPos = -1;
int targetIndex = -1; 
unsigned long lastBulletMoveTime = 0;

// 強力攻擊狀態
int powerShotCount = 0;     // 持有的強力子彈數量 (發射即減 1，達到 10 殺即加 1)
bool isPowerShotActive = false; 
int powerBulletPos = -1;    

// 難度追蹤
int killCount = 0;
int currentInterval = START_INTERVAL;
int currentLevel = 0;

// -----------------------------------------------------------------------------
// Function Prototypes
// -----------------------------------------------------------------------------
void updateDifficulty();
void handleLightMovement();
void handleShooting();
void handlePowerShot();
void handleBulletMovement();
void handlePowerBulletMovement();
void drawLights();
void gameOverSequence();
void resetGame();
void handleRestart();
void playShootSound();


// -----------------------------------------------------------------------------
// Buzzer 音效函數
// -----------------------------------------------------------------------------
void playShootSound() {
  analogWriteFreq(1000);
  analogWrite(BUZZER_PIN, 512);
  delay(50);
  analogWrite(BUZZER_PIN, 0);
}


// -----------------------------------------------------------------------------
// setup() 初始設定
// -----------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  randomSeed(analogRead(A0));

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(POWER_BUTTON_PIN, INPUT_PULLUP); 

  pinMode(BUZZER_PIN, OUTPUT);
  analogWriteRange(1023);

  pixels.begin();
  pixels.show();

  Serial.println("\nGame Initialized on ESP8266.");
}

// -----------------------------------------------------------------------------
// loop() 迴圈執行
// -----------------------------------------------------------------------------
void loop() {
  if (isGameOver) {
    gameOverSequence();
    handleRestart();
    delay(10);
    return;
  }

  // 1. 處理燈光移動和生成
  if (millis() - lastMoveTime >= currentInterval) {
    handleLightMovement();
    lastMoveTime = millis();
  }

  // 2. 處理標準子彈射擊
  handleShooting();

  // 3. 處理強力攻擊射擊
  handlePowerShot();

  // 4. 處理子彈的移動
  handleBulletMovement();
  handlePowerBulletMovement();

  // 5. 繪製燈條
  drawLights();
}

// -----------------------------------------------------------------------------
// 難度計算函數 (修正: 根據累積擊殺數，計算玩家應持有的強力子彈總數)
// -----------------------------------------------------------------------------
void updateDifficulty() {
  // 難度等級提升邏輯
  if (killCount >= KILLS_PER_LEVEL)
  {
    currentLevel++;
    currentInterval -= SPEED_INCREMENT;
    if (currentInterval < MIN_INTERVAL) {
      currentInterval = MIN_INTERVAL;
    }

    powerShotCount++;
  }

}

// -----------------------------------------------------------------------------
// 處理燈光移動和生成 (保持不變)
// -----------------------------------------------------------------------------
void handleLightMovement() {
  if (pixelHP[0] > 0) {
    Serial.println("Game Over: Light reached Index 0!");
    isGameOver = true;
    return;
  }

  for (int i = 1; i < NUMPIXELS; i++) {
    pixelHP[i - 1] = pixelHP[i];
  }

  int randomHP = random(4);
  pixelHP[NUMPIXELS - 1] = randomHP;
}

// -----------------------------------------------------------------------------
// 處理標準子彈射擊 (保持不變)
// -----------------------------------------------------------------------------
void handleShooting() {
  buttonState = digitalRead(BUTTON_PIN);

  if (bulletPos == -1 && powerBulletPos == -1 && buttonState == LOW && lastButtonState == HIGH) {

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
      isPowerShotActive = false;
      lastBulletMoveTime = millis();
      playShootSound();
    }
  }

  lastButtonState = buttonState;
}

// -----------------------------------------------------------------------------
// 處理強力攻擊射擊 (D3) (保持不變)
// -----------------------------------------------------------------------------
void handlePowerShot() {
  powerButtonState = digitalRead(POWER_BUTTON_PIN);

  if (powerShotCount > 0 && bulletPos == -1 && powerBulletPos == -1 && powerButtonState == LOW && lastPowerButtonState == HIGH) {

    powerShotCount--; // 發射後立刻消耗 1 顆

    int foundTarget = -1;
    for (int i = 0; i < NUMPIXELS; i++) {
      if (pixelHP[i] > 0) {
        foundTarget = i;
        break;
      }
    }

    if (foundTarget != -1) {
      targetIndex = foundTarget; 
      powerBulletPos = 0;
      isPowerShotActive = true; 
      lastBulletMoveTime = millis();

      // 播放強力音效
      analogWriteFreq(2000); 
      analogWrite(BUZZER_PIN, 512);
      delay(100);
      analogWrite(BUZZER_PIN, 0);

      Serial.println("POWER SHOT FIRED!");
    }
  }

  lastPowerButtonState = powerButtonState;
}

// -----------------------------------------------------------------------------
// 處理標準子彈移動與命中 (保持不變)
// -----------------------------------------------------------------------------
void handleBulletMovement() {
  if (bulletPos == -1 || isPowerShotActive) return;

  if (millis() - lastBulletMoveTime >= BULLET_SPEED) { 
    
    bulletPos++;
    lastBulletMoveTime = millis();

    if (bulletPos >= targetIndex) {

      int finalTargetIndex = -1;

      if (pixelHP[targetIndex] > 0) {
        finalTargetIndex = targetIndex;
      } 
      else if (targetIndex > 0 && pixelHP[targetIndex - 1] > 0) {
        finalTargetIndex = targetIndex - 1;
      }
      
      if (finalTargetIndex != -1) {
        
        pixelHP[finalTargetIndex]--; 
        
        if (pixelHP[finalTargetIndex] == 0) {
          killCount++;
          updateDifficulty(); 
          Serial.print("TARGET KILLED at index ");
          Serial.print(finalTargetIndex);
          Serial.print("! Total Kills: ");
          Serial.println(killCount);
        } else {
          Serial.print("Target hit at index ");
          Serial.print(finalTargetIndex);
          Serial.print(". New HP: ");
          Serial.println(pixelHP[finalTargetIndex]);
        }
      } else {
        Serial.println("Bullet missed: Target escaped or already destroyed.");
      }
      
      bulletPos = -1;
      targetIndex = -1;
    }
  }
}


// -----------------------------------------------------------------------------
// 處理強力攻擊子彈移動與命中 (保持不變)
// -----------------------------------------------------------------------------
void handlePowerBulletMovement() {
  if (powerBulletPos == -1 || !isPowerShotActive) return;

  if (millis() - lastBulletMoveTime >= POWER_BULLET_SPEED) { 
    
    powerBulletPos++;
    lastBulletMoveTime = millis();

    if (powerBulletPos >= targetIndex) {

      int startHitIndex = -1;

      if (pixelHP[targetIndex] > 0) {
        startHitIndex = targetIndex;
      }
      else if (targetIndex > 0 && pixelHP[targetIndex - 1] > 0) {
        startHitIndex = targetIndex - 1;
      } else {
        Serial.println("Power Shot missed: Initial target escaped or already destroyed.");
        powerBulletPos = -1;
        targetIndex = -1;
        isPowerShotActive = false;
        return; 
      }

      int enemiesKilled = 0;
      for (int i = startHitIndex; i < NUMPIXELS && enemiesKilled < POWER_SHOT_KILL_COUNT; i++) {
        
        if (pixelHP[i] > 0) {
          killCount++; 
          pixelHP[i] = 0; 
          
          if (random(2) == 0) { 
            currentInterval += SPEED_INCREMENT*2; 
            if (currentInterval > START_INTERVAL) currentInterval = START_INTERVAL;
          }

          enemiesKilled++;
          updateDifficulty(); 
        }
      }
      Serial.print("POWER SHOT HIT! Starting at index ");
      Serial.print(startHitIndex);
      Serial.print(". Enemies eliminated: ");
      Serial.println(enemiesKilled);

      powerBulletPos = -1;
      targetIndex = -1;
      isPowerShotActive = false;
    }
  }
}


// -----------------------------------------------------------------------------
// 繪製燈條 (保持不變)
// -----------------------------------------------------------------------------
void drawLights() {
  // 1. 繪製背景 (敵人 HP)
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
  if (bulletPos != -1 && bulletPos < NUMPIXELS && !isPowerShotActive) {
    pixels.setPixelColor(bulletPos, BULLET_COLOR);
  }

  if (powerBulletPos != -1 && powerBulletPos < NUMPIXELS && isPowerShotActive) {
    // 強力攻擊特效：頭部強化拖尾 (3顆燈漸變粉紅)
    
    pixels.setPixelColor(powerBulletPos, POWER_SHOT_COLOR); 

    if (powerBulletPos > 0) {
      pixels.setPixelColor(powerBulletPos - 1, POWER_TAIL1_COLOR);
    }
    
    if (powerBulletPos > 1) {
      pixels.setPixelColor(powerBulletPos - 2, POWER_TAIL2_COLOR);
    }
  }

  // 3. 覆蓋繪製強力攻擊準備燈
  // 只要 powerShotCount > 0，指示燈就亮。
  if (powerShotCount > 0) {
    pixels.setPixelColor(0, POWER_READY_COLOR);
  }

  pixels.show();
}

// -----------------------------------------------------------------------------
// 遊戲結束序列 (保持不變)
// -----------------------------------------------------------------------------
void gameOverSequence() {
  for(int i=0; i < NUMPIXELS; i++) {
    pixels.setPixelColor(i, GAMEOVER_COLOR);
  }
  pixels.show();
}

// -----------------------------------------------------------------------------
// 重置遊戲狀態 (保持不變)
// -----------------------------------------------------------------------------
void resetGame() {
  Serial.println("--- Restarting Game ---");

  for(int i=0; i < NUMPIXELS; i++) {
    pixelHP[i] = 0;
  }
  pixels.clear();
  pixels.show();

  isGameOver = false;
  lastMoveTime = millis();

  bulletPos = -1;
  targetIndex = -1;
  lastBulletMoveTime = 0;

  powerShotCount = 0;
  isPowerShotActive = false;
  powerBulletPos = -1;

  killCount = 0;
  currentLevel = 0;
  currentInterval = START_INTERVAL;

  buttonPressStartTime = 0;
  isButtonHeld = false;
  lastButtonState = HIGH;
  buttonState = HIGH;
  lastPowerButtonState = HIGH; 
  powerButtonState = HIGH;

  Serial.print("Game Reset. Start Interval: ");
  Serial.print(currentInterval);
  Serial.println("ms");
}

// -----------------------------------------------------------------------------
// 處理長按重新開始 (D1) (保持不變)
// -----------------------------------------------------------------------------
void handleRestart() {
  buttonState = digitalRead(BUTTON_PIN);
  unsigned long currentTime = millis();

  if (buttonState == LOW && lastButtonState == HIGH) {
    buttonPressStartTime = currentTime;
    isButtonHeld = true;
  }
  else if (buttonState == HIGH && lastButtonState == LOW) {
    buttonPressStartTime = 0;
    isButtonHeld = false;
  }

  if (isButtonHeld && (currentTime - buttonPressStartTime >= RESTART_HOLD_TIME)) {
    Serial.println("Long press detected! Restarting game...");
    resetGame();
    isButtonHeld = false;
    buttonPressStartTime = 0;
  }

  lastButtonState = buttonState;
}
