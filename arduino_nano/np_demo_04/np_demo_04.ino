#include <Adafruit_NeoPixel.h>

// -----------------------------------------------------------------------------
// 📌 參數設定
// -----------------------------------------------------------------------------
#define PIN             2      // Neopixel 數據腳位 (D2)
#define BUTTON_PIN      4      // 子彈按鈕腳位 (D4)
#define NUMPIXELS       20     // 燈條上的燈珠數量 (Index 0 - 19)
#define MOVE_INTERVAL   1000   // 燈光移動/生成的時間間隔 (1000ms = 1秒)
#define BULLET_SPEED    20     // 子彈每移動一格的延遲時間 (毫秒)

// 定義顏色
const uint32_t GAMEOVER_COLOR = Adafruit_NeoPixel::Color(20, 0, 0); // 紅色 (R=20)
const uint32_t COLOR_HP1      = Adafruit_NeoPixel::Color(0, 20, 0);   // 綠燈 (HP 1)
const uint32_t COLOR_HP2      = Adafruit_NeoPixel::Color(0, 0, 20);   // 藍燈 (HP 2)
const uint32_t COLOR_HP3      = Adafruit_NeoPixel::Color(20, 20, 20); // 白燈 (HP 3)
const uint32_t BULLET_COLOR   = Adafruit_NeoPixel::Color(30, 30, 0);  // 黃燈 (子彈)

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
int bulletPos = -1;             // -1: 子彈未發射或已命中
int targetIndex = -1;           // 子彈目標燈的 Index
unsigned long lastBulletMoveTime = 0; // 記錄子彈上次移動的時間


// -----------------------------------------------------------------------------
// ⚙️ setup() 初始設定
// -----------------------------------------------------------------------------
void setup() {
  Serial.begin(9600);
  randomSeed(analogRead(A0));
  
  pinMode(BUTTON_PIN, INPUT_PULLUP); 

  pixels.begin();
  pixels.show(); 
}

// -----------------------------------------------------------------------------
// 🔄 loop() 迴圈執行
// -----------------------------------------------------------------------------
void loop() {
  if (isGameOver) {
    gameOverSequence();
    return;
  }

  // 1. 處理燈光移動和生成 (每 MOVE_INTERVAL 執行一次)
  if (millis() - lastMoveTime >= MOVE_INTERVAL) {
    handleLightMovement();
    lastMoveTime = millis();
  }

  // 2. 處理子彈射擊 (按鈕按下時)
  handleShooting();

  // **新增**：3. 處理子彈的移動
  handleBulletMovement();

  // 4. 繪製燈條
  drawLights();
}

// -----------------------------------------------------------------------------
// 🎯 處理燈光移動和生成 (每秒執行)
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
  int randomHP = random(4); // 產生 0, 1, 2, 或 3
  pixelHP[NUMPIXELS - 1] = randomHP; 
  // Serial.print("New light generated at Index 19 with HP: ");
  // Serial.println(randomHP);
}


// -----------------------------------------------------------------------------
// 🔫 處理子彈射擊 (按鈕D4) - 這次只發射子彈，不直接傷害目標
// -----------------------------------------------------------------------------
void handleShooting() {
  buttonState = digitalRead(BUTTON_PIN);

  // 只有在子彈未發射中時，才能發射新子彈 (bulletPos == -1)
  if (bulletPos == -1 && buttonState == LOW && lastButtonState == HIGH) {
    
    // 尋找最靠近玩家的目標 Index
    int foundTarget = -1;
    for (int i = 0; i < NUMPIXELS; i++) {
      if (pixelHP[i] > 0) {
        foundTarget = i;
        break; 
      }
    }
    
    if (foundTarget != -1) {
      // 找到目標，發射子彈
      targetIndex = foundTarget; // 設定目標
      bulletPos = 0;             // 子彈從 Index 0 開始
      lastBulletMoveTime = millis(); // 重設子彈移動計時器
      Serial.print("Bullet fired! Target Index: ");
      Serial.println(targetIndex);
    }
  }

  lastButtonState = buttonState;
}

// -----------------------------------------------------------------------------
// 💥 處理子彈移動與命中
// -----------------------------------------------------------------------------
void handleBulletMovement() {
  if (bulletPos == -1) return; // 子彈未發射

  // 檢查是否到達移動時間
  if (millis() - lastBulletMoveTime >= BULLET_SPEED) {
    
    // 子彈移動一格
    bulletPos++;
    lastBulletMoveTime = millis();

    // 檢查子彈是否命中目標
    if (bulletPos >= targetIndex) {
      
      // 命中目標，減少生命值
      if (pixelHP[targetIndex] > 0) {
        pixelHP[targetIndex]--; 
        Serial.print("Target hit at Index: ");
        Serial.print(targetIndex);
        Serial.print(", New HP: ");
        Serial.println(pixelHP[targetIndex]);
      }
      
      // 重設子彈狀態
      bulletPos = -1;
      targetIndex = -1;
    }
  }
}


// -----------------------------------------------------------------------------
// 🎨 繪製燈條 (加入子彈優先繪製邏輯)
// -----------------------------------------------------------------------------
void drawLights() {
  // 1. 繪製背景 (燈光目標)
  for (int i = 0; i < NUMPIXELS; i++) {
    uint32_t colorToSet = 0; // 預設熄滅
    
    switch (pixelHP[i]) {
      case 3: colorToSet = COLOR_HP3; break;
      case 2: colorToSet = COLOR_HP2; break;
      case 1: colorToSet = COLOR_HP1; break;
      case 0: 
      default: colorToSet = 0; break;
    }

    pixels.setPixelColor(i, colorToSet);
  }
  
  // 2. **覆蓋**繪製子彈
  if (bulletPos != -1 && bulletPos < NUMPIXELS) {
    // 確保子彈位置有效且在範圍內
    pixels.setPixelColor(bulletPos, BULLET_COLOR); 
  }

  pixels.show(); // 更新燈條
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
