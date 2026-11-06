import json
import pygame
import sys
import time

# --- 設定 ---
CELL_SIZE = 40  # 1マスのピクセルサイズ
FPS = 5         # 1秒間に進むステップ数（アニメーション速度）

# 色の定義 (R, G, B)
COLORS = {
    'w': (100, 100, 100),  # 壁（グレー）
    '.': (240, 240, 240),  # 通路（薄いグレー）
    'o': (200, 220, 255),  # 待機所（薄い青）
    's': (220, 180, 120),  # 棚（茶色）
    'r': (255, 200, 200)   # 完了地（薄い赤）
}
AGV_COLOR = (255, 50, 50)  # AGV（赤）

def main():
    # 1. JSONデータの読み込み
    try:
        with open("output/history.json", "r") as f:
            data = json.load(f)
    except FileNotFoundError:
        print("エラー: output/history.json が見つかりません。先にC++を実行してください。")
        return

    grid = data["map"]
    history = data["history"]

    width = len(grid[0])
    height = len(grid)

    # 2. Pygame初期化
    pygame.init()
    screen = pygame.display.set_mode((width * CELL_SIZE, height * CELL_SIZE))
    pygame.display.set_caption("ASRS Simulator - Phase 1")
    clock = pygame.time.Clock()

    step = 0
    running = True

    # 3. メインループ
    while running:
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                running = False

        screen.fill((0, 0, 0))

        # マップの描画
        for y, row in enumerate(grid):
            for x, cell in enumerate(row):
                color = COLORS.get(cell, (0,0,0))
                rect = pygame.Rect(x * CELL_SIZE, y * CELL_SIZE, CELL_SIZE, CELL_SIZE)
                pygame.draw.rect(screen, color, rect)
                pygame.draw.rect(screen, (200,200,200), rect, 1) # グリッド線

        # AGVの描画
        if step < len(history):
            agvs_current = history[step]
            for agv in agvs_current:
                center_x = int(agv["x"] * CELL_SIZE + CELL_SIZE / 2)
                center_y = int(agv["y"] * CELL_SIZE + CELL_SIZE / 2)
                pygame.draw.circle(screen, AGV_COLOR, (center_x, center_y), int(CELL_SIZE * 0.4))
                
                # IDの描画
                font = pygame.font.SysFont(None, 24)
                text = font.render(str(agv["id"]), True, (255, 255, 255))
                text_rect = text.get_rect(center=(center_x, center_y))
                screen.blit(text, text_rect)
            
            # ステップ数を進める
            step += 1
        else:
            # 終了したらループ（最初から再生）
            time.sleep(1)
            step = 0

        pygame.display.flip()
        clock.tick(FPS)

    pygame.quit()
    sys.exit()

if __name__ == "__main__":
    main()