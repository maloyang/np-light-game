# microbit-game.py (修訂版)
# 程式平台: https://python.microbit.org/v/3
# 執行前請確認已安裝 microbit 上的 neopixel 函式庫

from microbit import *
import neopixel
import random
import music  # 增加匯入 music 函式庫以使用內建 Buzzer

# -----------------------------------------------------------------------------
# 參數設定
# -----------------------------------------------------------------------------
# Micro:bit 腳位對應: P1 控制 NeoPixel, P2 作為按鈕 (現改為 Button A)
PIN_NEOPIXEL = pin1
# PIN_BUTTON = pin2  # 移除外部按鈕
PIN_BUZZER = pin0 # P0 腳位，用於外接 Buzzer (若為 V1) 或作為 music 函式的預設輸出

NUM_PIXELS = 20
BULLET_SPEED = 20
START_INTERVAL = 1000
MIN_INTERVAL = 200
SPEED_INCREMENT = 50
KILLS_PER_LEVEL = 5

# 定義顏色 (R, G, B) - Micro:bit NeoPixel 顏色值範圍是 0-255
GAMEOVER_COLOR = (200, 0, 0)
# 敵人 1 生命: 綠色 (Green)
COLOR_HP1      = (0, 200, 0)
# 敵人 2 生命: 藍色 (Blue)
COLOR_HP2      = (0, 0, 200)
# COLOR_HP3      = (200, 200, 200) # 移除 3 生命敵人
# 子彈顏色: 黃色 (Yellow)
BULLET_COLOR   = (255, 255, 0)

# -----------------------------------------------------------------------------
# 狀態變數
# -----------------------------------------------------------------------------
# 初始化 NeoPixel
np = neopixel.NeoPixel(PIN_NEOPIXEL, NUM_PIXELS)

# 遊戲狀態
pixel_hp = [0] * NUM_PIXELS
last_move_time = 0
is_game_over = False
# last_button_state = 1 # 移除外部按鈕狀態追蹤

# 子彈狀態
bullet_pos = -1
target_index = -1
last_bullet_move_time = 0

# 難度追蹤
kill_count = 0
current_interval = START_INTERVAL
current_level = 0


# -----------------------------------------------------------------------------
# 難度計算函數 (不變)
# -----------------------------------------------------------------------------
def update_difficulty():
    global current_level, current_interval, kill_count

    new_level = kill_count // KILLS_PER_LEVEL

    if new_level > current_level:
        current_level = new_level

        time_reduction = current_level * SPEED_INCREMENT
        current_interval = START_INTERVAL - time_reduction

        if current_interval < MIN_INTERVAL:
            current_interval = MIN_INTERVAL

        #display.scroll("LV" + str(current_level) + " SPD" + str(current_interval))

# -----------------------------------------------------------------------------
# 處理燈光移動和生成
# -----------------------------------------------------------------------------
def handle_light_movement():
    global is_game_over, pixel_hp

    # A. 檢查遊戲結束條件 (Index 0 的燈是否亮著)
    if pixel_hp[0] > 0:
        is_game_over = True
        return

    # B. 燈光移動 (i -> i-1)
    for i in range(1, NUM_PIXELS):
        pixel_hp[i - 1] = pixel_hp[i]

    # C. 隨機生成新燈 (在 Index 19)
    # 敵人只有 1 生命 (綠色, HP=1) 或 2 生命 (藍色, HP=2)
    # 隨機生成 0 (空), 1 (綠), 2 (藍)
    random_hp = random.randint(0, 2)
    pixel_hp[NUM_PIXELS - 1] = random_hp


# -----------------------------------------------------------------------------
# 處理子彈射擊 (改為按鈕 A)
# -----------------------------------------------------------------------------
def handle_shooting():
    global bullet_pos, target_index, last_bullet_move_time

    # 射擊條件: 子彈不在飛行中 且 按鈕 A 被按下
    if bullet_pos == -1 and button_a.is_pressed():

        found_target = -1
        # 尋找最接近玩家的敵人 (最低的 index)
        for i in range(NUM_PIXELS):
            if pixel_hp[i] > 0:
                found_target = i
                break

        if found_target != -1:
            # 找到目標, 開始射擊
            target_index = found_target
            bullet_pos = 0
            last_bullet_move_time = running_time()
            
            # 發出「及」一聲
            music.play(music.BA_DING, wait=False)
            sleep(10) # 保持極短的延遲以模擬快速的「及」聲
            music.stop()


# -----------------------------------------------------------------------------
# 處理子彈移動與命中 (不變)
# -----------------------------------------------------------------------------
def handle_bullet_movement():
    global bullet_pos, target_index, last_bullet_move_time, kill_count, pixel_hp

    if bullet_pos == -1:
        return

    if running_time() - last_bullet_move_time >= BULLET_SPEED:

        bullet_pos += 1
        last_bullet_move_time = running_time()

        if bullet_pos >= target_index:

            if pixel_hp[target_index] > 0:
                pixel_hp[target_index] -= 1

                if pixel_hp[target_index] == 0:
                    kill_count += 1
                    update_difficulty()

            bullet_pos = -1
            target_index = -1


# -----------------------------------------------------------------------------
# 繪製燈條 (HP 顏色對應調整)
# -----------------------------------------------------------------------------
def draw_lights():
    # 1. 繪製背景 (燈光目標)
    for i in range(NUM_PIXELS):
        color_to_set = (0, 0, 0)

        hp = pixel_hp[i]
        # if hp == 3:
        #     color_to_set = COLOR_HP3 # 移除 3 生命
        if hp == 2:
            color_to_set = COLOR_HP2
        elif hp == 1:
            color_to_set = COLOR_HP1

        np[i] = color_to_set

    # 2. 覆蓋繪製子彈 (顏色已設定為黃色)
    if bullet_pos != -1 and bullet_pos < NUM_PIXELS:
        np[bullet_pos] = BULLET_COLOR

    np.show()

# -----------------------------------------------------------------------------
# 遊戲結束序列 (不變)
# -----------------------------------------------------------------------------
def game_over_sequence():
    display.show(Image.SAD)

    for i in range(NUM_PIXELS):
        np[i] = GAMEOVER_COLOR
        np.show()
        sleep(50)

    sleep(100)

    while True:
        np.clear()
        np.show()
        sleep(300)
        for i in range(NUM_PIXELS):
            np[i] = GAMEOVER_COLOR
        np.show()
        sleep(300)
        if button_a.is_pressed(): # 使用按鈕 A 重啟
            break

# -----------------------------------------------------------------------------
# 主迴圈
# -----------------------------------------------------------------------------
#display.scroll("NEOPIXEL GAME START")
display.show(Image('00900:'
                   '00900:'
                   '00900:'
                   '09990:'
                   '09990'))
# PIN_BUTTON.set_pull(PIN_BUTTON.PULL_UP) # 移除外部按鈕設定

while True:
    if is_game_over:
        game_over_sequence()
        # 重設遊戲狀態以便重新開始
        pixel_hp = [0] * NUM_PIXELS
        last_move_time = 0
        is_game_over = False
        bullet_pos = -1
        target_index = -1
        kill_count = 0
        current_level = 0
        current_interval = START_INTERVAL
        display.show(Image.HAPPY) # 重新開始顯示笑臉
        sleep(500)
        continue
        
    current_time = running_time()

    # 1. 處理燈光移動和生成
    if current_time - last_move_time >= current_interval:
        handle_light_movement()
        last_move_time = current_time

    # 2. 處理子彈射擊 (使用 Button A)
    handle_shooting()

    # 3. 處理子彈的移動
    handle_bullet_movement()

    # 4. 繪製燈條
    draw_lights()

    sleep(1)