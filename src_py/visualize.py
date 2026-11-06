import json
import tkinter as tk

# --- 設定 ---
CELL_SIZE = 40  # 1マスのピクセルサイズ
FPS_MS = 200    # 1ステップの表示時間（ミリ秒）200ms = 5FPS

# 色の定義 (Tkinterで使えるカラーネームまたはカラーコード)
COLORS = {
    'w': 'gray',        # 壁
    '.': 'white',       # 通路
    'o': 'lightblue',   # 待機所
    's': 'orange',      # 棚
    'r': 'pink'         # 完了地
}
AGV_COLOR = 'red'

def main():
    # 1. JSONデータの読み込み
    try:
        with open("output/history.json", "r") as f:
            data = json.load(f)
    except FileNotFoundError:
        print("エラー: output/history.json が見つかりません。")
        return

    grid = data["map"]
    history = data["history"]

    width = len(grid[0])
    height = len(grid)

    # 2. Tkinterウィンドウとキャンバスの準備
    root = tk.Tk()
    root.title("ASRS Simulator - Tkinter Version")
    
    # 描画領域（キャンバス）の作成
    canvas = tk.Canvas(root, width=width * CELL_SIZE, height=height * CELL_SIZE)
    canvas.pack()

    # 3. マップの描画（背景として1回だけ描く）
    for y, row in enumerate(grid):
        for x, cell in enumerate(row):
            color = COLORS.get(cell, 'black')
            x1, y1 = x * CELL_SIZE, y * CELL_SIZE
            x2, y2 = x1 + CELL_SIZE, y1 + CELL_SIZE
            canvas.create_rectangle(x1, y1, x2, y2, fill=color, outline='lightgray')

    # 4. アニメーション関数
    def animate(step):
        # 以前のステップのAGVを消去（"agv"というタグがついた図形だけ消す）
        canvas.delete("agv")

        # 現在のステップのAGVを描画
        if step < len(history):
            for agv in history[step]:
                cx = agv["x"] * CELL_SIZE + CELL_SIZE / 2
                cy = agv["y"] * CELL_SIZE + CELL_SIZE / 2
                r = CELL_SIZE * 0.4
                
                # 丸（AGV）を描画
                canvas.create_oval(cx - r, cy - r, cx + r, cy + r, fill=AGV_COLOR, tags="agv")
                # ID（文字）を描画
                canvas.create_text(cx, cy, text=str(agv["id"]), fill="white", tags="agv")
            
            # 指定時間（FPS_MS）後に、次のステップを呼び出す
            root.after(FPS_MS, animate, step + 1)
        else:
            # 終了したら少し待って最初からループ
            root.after(1000, animate, 0)

    # アニメーション開始（ステップ0から）
    animate(0)

    # ウィンドウを表示し続ける
    root.mainloop()

if __name__ == "__main__":
    main()