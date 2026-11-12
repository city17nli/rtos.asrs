import json
import tkinter as tk

# --- 設定 ---
CELL_SIZE = 40      # 1マスのピクセルサイズ
FPS_MS = 200        # 1ステップの表示時間（ミリ秒）200ms = 5FPS
PANEL_HEIGHT = 90   # マップの下に作る情報パネルの高さ
FONT = ("", 9)

# マップの色 (Tkinterで使えるカラーネームまたはカラーコード)
COLORS = {
    'w': 'gray',        # 壁
    '.': 'white',       # 通路
    'o': 'lightblue',   # 待機所
    's': 'orange',      # 棚
    'r': 'pink'         # 完了地
}

# AGVの状態ごとの色（main.cpp の AGVState と対応: 0=待機, 1=受け取りへ, 2=配送中）
AGV_COLORS = {0: 'gray40', 1: 'dodgerblue', 2: 'orangered'}

# タスクの色
TASK_UNASSIGNED_COLOR = 'gold'       # 未割当
TASK_ASSIGNED_COLOR = 'limegreen'    # 割り当て済み（AGVが受け取りに向かっている）
DESTINATION_COLOR = 'orangered'      # 配送中の荷物の届け先

# タスクの状態（visualize.py 内で使う名前）
UNASSIGNED = 'unassigned'   # 未割当
TO_PICKUP = 'to_pickup'     # 割り当て済み・荷物を受け取りに向かっている
CARRYING = 'carrying'       # 荷物を積んで配送中
DONE = 'done'               # 完了


def compute_task_states(tasks, history):
    """各ステップでの各タスクの状態を、AGVの記録から割り出す。

    task_states[step][タスクid] = 'unassigned' / 'to_pickup' / 'carrying' / 'done'
    - どこかのAGVが担当している → 状態2なら carrying、それ以外は to_pickup
    - 担当AGVがいない → まだ誰にも担当されていなければ unassigned、
      一度でも担当されたあとなら done
    """
    task_states = []
    ever_assigned = set()  # これまでに一度でも担当されたタスクid

    for step_agvs in history:
        # このステップで担当されているタスクを集める
        active = {}
        for agv in step_agvs:
            tid = agv.get("task", -1)
            if tid != -1:
                if agv.get("state", 0) == 2:
                    active[tid] = CARRYING
                else:
                    active[tid] = TO_PICKUP
                ever_assigned.add(tid)

        # 全タスクの状態を決める
        states = {}
        for task in tasks:
            tid = task["id"]
            if tid in active:
                states[tid] = active[tid]
            elif tid in ever_assigned:
                states[tid] = DONE
            else:
                states[tid] = UNASSIGNED
        task_states.append(states)

    return task_states


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
    tasks = data.get("tasks", [])  # tasks が無い古いJSONでも動くようにする

    width = len(grid[0])
    height = len(grid)
    map_pixel_height = height * CELL_SIZE

    # 各ステップのタスクの状態を先に計算しておく
    task_states = compute_task_states(tasks, history)

    # 2. Tkinterウィンドウとキャンバスの準備
    root = tk.Tk()
    root.title("ASRS Simulator - Tkinter Version")

    # マップ + 下の情報パネル分の高さを確保
    canvas = tk.Canvas(root, width=width * CELL_SIZE,
                       height=map_pixel_height + PANEL_HEIGHT)
    canvas.pack()

    # マス (x, y) の四角形の座標を返す（pad で内側に縮める）
    def cell_box(x, y, pad=0):
        return (x * CELL_SIZE + pad, y * CELL_SIZE + pad,
                (x + 1) * CELL_SIZE - pad, (y + 1) * CELL_SIZE - pad)

    # 3. マップの描画（背景として1回だけ描く）
    for y, row in enumerate(grid):
        for x, cell in enumerate(row):
            color = COLORS.get(cell, 'black')
            x1, y1, x2, y2 = cell_box(x, y)
            canvas.create_rectangle(x1, y1, x2, y2, fill=color, outline='lightgray')

    # 4. 情報パネルの凡例（1回だけ描く）
    def legend_circle(x, y, color, label):
        canvas.create_oval(x, y - 8, x + 16, y + 8, fill=color, outline='')
        canvas.create_text(x + 22, y, text=label, anchor='w', font=FONT)

    def legend_square(x, y, fill, outline, label):
        canvas.create_rectangle(x, y - 7, x + 16, y + 7, fill=fill, outline=outline, width=2)
        canvas.create_text(x + 22, y, text=label, anchor='w', font=FONT)

    panel_top = map_pixel_height
    canvas.create_line(0, panel_top, width * CELL_SIZE, panel_top, fill='black')

    # 1段目: AGVの色
    legend_circle(10, panel_top + 45, AGV_COLORS[0], "AGV 待機")
    legend_circle(150, panel_top + 45, AGV_COLORS[1], "AGV 受け取りへ")
    legend_circle(310, panel_top + 45, AGV_COLORS[2], "AGV 配送中(荷物あり)")
    # 2段目: タスクの色
    legend_square(10, panel_top + 70, TASK_UNASSIGNED_COLOR, 'black', "未割当タスク(棚)")
    legend_square(170, panel_top + 70, TASK_ASSIGNED_COLOR, 'black', "割当済みタスク(棚)")
    legend_square(340, panel_top + 70, '', DESTINATION_COLOR, "配送先")

    # 5. 描画用の関数（毎ステップ呼ばれる。図形には "dyn" タグを付けて、次のステップで消す）
    def draw_tasks(step):
        for task in tasks:
            tid = task["id"]
            state = task_states[step][tid]

            if state == UNASSIGNED or state == TO_PICKUP:
                # まだ荷物が棚にある → 棚に色を付けてタスクidを表示
                color = TASK_UNASSIGNED_COLOR if state == UNASSIGNED else TASK_ASSIGNED_COLOR
                sx, sy = task["shelf"]
                x1, y1, x2, y2 = cell_box(sx, sy)
                canvas.create_rectangle(x1, y1, x2, y2, fill=color, outline='black', tags="dyn")
                canvas.create_text((x1 + x2) / 2, (y1 + y2) / 2, text="T" + str(tid),
                                   font=FONT, tags="dyn")

                # 荷物を受け取るマスを枠線で示す
                px, py = task["pickup"]
                x1, y1, x2, y2 = cell_box(px, py, 3)
                canvas.create_rectangle(x1, y1, x2, y2, outline=color, width=3, tags="dyn")

            elif state == CARRYING:
                # 荷物はAGVの上 → 配送先のマスを枠線で示す
                dx, dy = task["destination"]
                x1, y1, x2, y2 = cell_box(dx, dy, 3)
                canvas.create_rectangle(x1, y1, x2, y2, outline=DESTINATION_COLOR,
                                        width=3, tags="dyn")
            # DONE のときは何も描かない

    def draw_agvs(step):
        for agv in history[step]:
            state = agv.get("state", 0)
            cx = agv["x"] * CELL_SIZE + CELL_SIZE / 2
            cy = agv["y"] * CELL_SIZE + CELL_SIZE / 2
            r = CELL_SIZE * 0.4

            # 丸（AGV）を状態に応じた色で描画
            canvas.create_oval(cx - r, cy - r, cx + r, cy + r,
                               fill=AGV_COLORS.get(state, 'red'), tags="dyn")
            # ID（文字）を描画
            canvas.create_text(cx, cy, text=str(agv["id"]), fill="white", tags="dyn")

            # 荷物を積んでいるときは、右上に黄色い四角（荷物）を付ける
            if state == 2:
                s = CELL_SIZE * 0.12
                bx = cx + r * 0.6
                by = cy - r * 0.8
                canvas.create_rectangle(bx - s, by - s, bx + s, by + s,
                                        fill=TASK_UNASSIGNED_COLOR, outline='black', tags="dyn")

    def draw_status(step):
        states = task_states[step].values()
        done = sum(1 for s in states if s == DONE)
        waiting = sum(1 for s in states if s == UNASSIGNED)
        working = len(task_states[step]) - done - waiting
        text = "ステップ {}/{}    タスク: 完了 {} / 全体 {}    未割当 {}    進行中 {}".format(
            step + 1, len(history), done, len(tasks), waiting, working)
        canvas.create_text(10, panel_top + 18, text=text, anchor='w',
                           font=("", 10, "bold"), tags="dyn")

    # 6. アニメーション関数
    def animate(step):
        # 以前のステップの図形を消去（"dyn" というタグがついた図形だけ消す）
        canvas.delete("dyn")

        if step < len(history):
            # タスク → AGV の順に描く（AGVを一番上に重ねる）
            draw_tasks(step)
            draw_agvs(step)
            draw_status(step)

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