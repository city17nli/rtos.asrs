"""simulationのJSONを読み込んでアニメーション表示する。
使い方(rtos.asrs直下で):
    python src_py/visualize.py                       # output/sim_greedy.json を表示
    python src_py/visualize.py output/xxx.json       # 別のJSONを表示
    python src_py/visualize.py --save output/demo.gif  # GIFとして保存(スライド貼り付け用)
必要: pip install matplotlib (GIF保存にはPillowも必要)
"""
import argparse
import json

import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation, PillowWriter
from matplotlib.patches import Circle, Rectangle

CELL_COLOR = {"w": "#444444", "s": "#b08850", ".": "#ffffff", "o": "#bfe3ff", "r": "#8fd694"}
STATE_COLOR = {"moving": "#3b82f6", "returning": "#a855f7", "waiting": "#f59e0b", "done": "#22c55e"}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("json_path", nargs="?", default="output/sim_greedy.json")
    ap.add_argument("--save", default=None, help="GIFの保存先")
    ap.add_argument("--interval", type=int, default=500, help="1tickの表示時間[ms]")
    args = ap.parse_args()

    with open(args.json_path, encoding="utf-8") as f:
        data = json.load(f)
    grid, W, H = data["map"], data["width"], data["height"]
    frames, result = data["frames"], data["result"]

    fig, ax = plt.subplots(figsize=(W * 0.75, H * 0.75 + 0.8))
    ax.set_xlim(0, W)
    ax.set_ylim(H, 0)  # y軸は下向き(マップのテキストと同じ見た目)
    ax.set_aspect("equal")
    ax.axis("off")

    # 背景(マップ)
    for y, row in enumerate(grid):
        for x, c in enumerate(row):
            ax.add_patch(Rectangle((x, y), 1, 1, fc=CELL_COLOR.get(c, "#fff"), ec="#999999", lw=0.5))
            if c == "r":
                ax.text(x + 0.5, y + 0.5, "R", ha="center", va="center", fontsize=9, color="#1b5e20")

    title = ax.set_title("", fontsize=13)
    artists = []  # 毎フレーム描き直すAGV用オブジェクト

    def draw(i):
        for a in artists:
            a.remove()
        artists.clear()
        fr = frames[i]
        for a in fr["agvs"]:
            c = Circle((a["x"] + 0.5, a["y"] + 0.5), 0.38, fc=STATE_COLOR[a["state"]], ec="black", lw=1)
            ax.add_patch(c)
            t = ax.text(a["x"] + 0.5, a["y"] + 0.5, str(a["id"]), ha="center", va="center",
                        color="white", fontsize=10, fontweight="bold")
            artists.extend([c, t])
        last = i == len(frames) - 1
        if last and result["finished"]:
            title.set_text(f"t = {fr['t']}   all AGVs returned")
            title.set_color("green")
        elif last:
            title.set_text(f"t = {fr['t']}   STUCK  (not finished)")
            title.set_color("red")
        else:
            title.set_text(f"t = {fr['t']}")
            title.set_color("black")
        return artists

    # 凡例
    for k, (name, col) in enumerate(STATE_COLOR.items()):
        ax.add_patch(Circle((0.6 + k * 2.4, H + 0.45), 0.2, fc=col, ec="black", clip_on=False))
        ax.text(0.95 + k * 2.4, H + 0.45, name, va="center", fontsize=9)

    # 最後のフレームで少し止まって見えるように末尾を繰り返す
    idx = list(range(len(frames))) + [len(frames) - 1] * 6
    anim = FuncAnimation(fig, lambda k: draw(idx[k]), frames=len(idx), interval=args.interval, repeat=False)

    if args.save:
        anim.save(args.save, writer=PillowWriter(fps=max(1, 1000 // args.interval)))
        print("saved:", args.save)
    else:
        plt.show()


if __name__ == "__main__":
    main()