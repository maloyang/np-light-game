#include <Adafruit_NeoPixel.h>

// -----------------------------------------------------------------------------
// 📌 參數設定
// -----------------------------------------------------------------------------
#define PIN             2      // Neopixel 數據腳位 (D2)
#define BUTTON_PIN      4      // 子彈按鈕腳位 (D4)
#define NUMPIXELS       20     // 燈條上的燈珠數量 (Index 0 - 19)
#define MOVE_INTERVAL   1000   // 燈光移動/生成的時間間隔 (1000ms = 1秒)

// 定義顏色 (中低亮度，可保護眼睛)
const uint32_t GAMEOVER_COLOR = Adafruit_NeoPixel::Color(20, 0, 0); // 紅色 (R=20)
const uint32_t COLOR_HP1      = Adafruit_NeoPixel::Color(0, 20, 0); // 綠燈 (HP 1)
const uint32_t COLOR_HP2      = Adafruit_NeoPixel::Color(0, 0, 20); // 藍燈 (HP 2)
const uint32_t COLOR_HP3      = Adafruit_NeoPixel::Color(20, 20, 20); // 白燈 (HP 3)

// -----------------------------------------------------------------------------
// 💾 狀態變數
// -----------------------------------------------------------------------------
Adafruit_NeoPixel pixels(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);

// 記錄每顆燈的生命值 (0=熄滅, 1=綠, 2=藍, 3=白)
// Index 0 是最靠近玩家的燈。
int pixelHP[NUMPIXELS] = {0}; 

unsigned long lastMoveTime = 0; // 記錄上次移動/生成燈的時間
bool isGameOver = false;        // 遊戲狀態旗標
int buttonState = 0;            // 當前按鈕狀態
int lastButtonState = 0;        // 上次按鈕狀態

// -----------------------------------------------------------------------------
// ⚙️ setup() 初始設定
// -----------------------------------------------------------------------------
void setup() {
  Serial.begin(9600);
  randomSeed(analogRead(A0)); // 提高亂數的隨機性 (使用未連接的類比腳位 A0)
  
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

  // 3. 繪製燈條
  drawLights();
}

// -----------------------------------------------------------------------------
// 🎯 處理燈光移動和生成 (每秒執行)
// -----------------------------------------------------------------------------
void handleLightMovement() {
  
  // A. 檢查遊戲結束條件 (Index 0 的燈是否亮著，即生命值 > 0)
  if (pixelHP[0] > 0) {
    Serial.println("Game Over: Light reached Index 0!");
    isGameOver = true;
    return;
  }

  // B. 燈光移動 (i -> i-1)
  // 將 Index 19 的燈移動到 Index 18，Index 1 的燈移動到 Index 0，等等。
  for (int i = 1; i < NUMPIXELS; i++) {
    // 將前一個燈的生命值賦予給當前燈
    pixelHP[i - 1] = pixelHP[i]; 
  }

  // C. 隨機生成新燈 (在 Index 19)
  // 產生 0 到 3 的亂數: 0=熄滅, 1=綠燈(HP1), 2=藍燈(HP2), 3=白燈(HP3)
  int randomHP = random(4); // 產生 0, 1, 2, 或 3

  pixelHP[NUMPIXELS - 1] = randomHP; // 設定 Index 19 的生命值

  Serial.print("New light generated at Index 19 with HP: ");
  Serial.println(randomHP);
}

// -----------------------------------------------------------------------------
// 🔫 處理子彈射擊 (按鈕D4)
// -----------------------------------------------------------------------------
void handleShooting() {
  buttonState = digitalRead(BUTTON_PIN);

  // 檢測按鈕從 鬆開 (HIGH) 變成 按下 (LOW) 的瞬間 (下降緣觸發)
  if (buttonState == LOW && lastButtonState == HIGH) {
    
    // 尋找最小 Index 且生命值 > 0 的燈 (最靠近玩家的目標)
    for (int i = 0; i < NUMPIXELS; i++) {
      if (pixelHP[i] > 0) {
        // 找到目標，將其生命值 -1
        pixelHP[i]--; 
        Serial.print("Bullet fired! Hit target at Index: ");
        Serial.print(i);
        Serial.print(", New HP: ");
        Serial.println(pixelHP[i]);
        break; // 只擊滅或傷害一個目標
      }
    }
  }

  lastButtonState = buttonState;
}

// -----------------------------------------------------------------------------
// 🎨 繪製燈條 (根據生命值繪製顏色)
// -----------------------------------------------------------------------------
void drawLights() {
  for (int i = 0; i < NUMPIXELS; i++) {
    uint32_t colorToSet = 0; // 預設熄滅
    
    switch (pixelHP[i]) {
      case 3:
        colorToSet = COLOR_HP3; // 白燈 (HP 3)
        break;
      case 2:
        colorToSet = COLOR_HP2; // 藍燈 (HP 2)
        break;
      case 1:
        colorToSet = COLOR_HP1; // 綠燈 (HP 1)
        break;
      case 0:
      default:
        colorToSet = 0; // 熄滅 (HP 0)
        break;
    }

    pixels.setPixelColor(i, colorToSet);
  }
  pixels.show(); // 更新燈條
}

// -----------------------------------------------------------------------------
// 🛑 遊戲結束序列
// -----------------------------------------------------------------------------
void gameOverSequence() {
  // 全部燈亮紅色
  for(int i=0; i < NUMPIXELS; i++) {
    pixels.setPixelColor(i, GAMEOVER_COLOR);
  }
  pixels.show();
  
  // 保持紅燈狀態
  delay(100); 
}
