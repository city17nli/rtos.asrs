#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstdlib>
#include <ctime>

// 座標を扱う構造体
struct Position {
    int x, y;
};

// AGVの状態
enum AGVState {
    AGV_IDLE,       // 待機（タスクなし。待機所へ戻る途中も含む）
    AGV_TO_PICKUP,  // 荷物を受け取りに棚へ向かっている
    AGV_TO_DELIVER  // 荷物を運んで配送口へ向かっている
};

// タスクの状態
enum TaskState {
    TASK_UNASSIGNED, // 未割当
    TASK_ASSIGNED,   // 割り当て済み
    TASK_DONE        // 完了
};

// AGVクラス（コンストラクタなし）
class AGV {
public:
    int id;
    AGVState state;
    Position pos;         // 現在地
    Position destination; // 今向かっている先
    int task_id;          // 担当中のタスクid（なければ -1）
    Position home;        // 待機所（暇なときに戻る場所）
};

// タスククラス
class Task {
public:
    int id;
    Position shelf;       // 荷物がある棚のマス
    Position pickup;      // AGVが荷物を受け取るマス（棚の左右の通路）
    Position destination; // 配送先（配送口 'r'）
    TaskState state;
    int agv_id;           // 担当AGVのid（なければ -1）
    int created_step;     // タスクが発生したステップ
};

// シミュレータ全体を管理するクラス
class Simulator {
private:
    std::vector<std::string> grid;
    std::vector<AGV> agvs;
    std::vector<Task> tasks;
    int map_width, map_height;
    int spawn_percent; // 1ステップごとにタスクが発生する確率（%）

    // 移動方向 上, 下, 左, 右
    int dx[4] = {0, 0, -1, 1};
    int dy[4] = {-1, 1, 0, 0};

    // 通れるマスか判定（範囲内で、壁 'w' でも棚 's' でもない）
    bool canPass(int x, int y) {
        if (x < 0 || x >= map_width || y < 0 || y >= map_height) return false;
        if (grid[y][x] == 'w' || grid[y][x] == 's') return false;
        return true;
    }

    // そのマスに他のAGVがいるか判定
    bool isOccupied(int x, int y) {
        for (size_t i = 0; i < agvs.size(); ++i) {
            if (agvs[i].pos.x == x && agvs[i].pos.y == y) return true;
        }
        return false;
    }

    // タスクidから、tasks の何番目かを探す（見つからなければ -1）
    int findTaskIndex(int task_id) {
        for (size_t i = 0; i < tasks.size(); ++i) {
            if (tasks[i].id == task_id) return i;
        }
        return -1;
    }

    // target から各マスまでの歩数を計算する（幅優先探索。行けないマスは -1）
    // 壁と棚だけを障害物とし、他のAGVの位置は考えない
    std::vector<std::vector<int> > calcDistance(Position target) {
        std::vector<std::vector<int> > dist(map_height, std::vector<int>(map_width, -1));
        std::vector<Position> todo; // 調べる予定のマスのリスト

        dist[target.y][target.x] = 0;
        todo.push_back(target);

        for (size_t head = 0; head < todo.size(); ++head) {
            Position cur = todo[head];
            for (int dir = 0; dir < 4; ++dir) {
                int nx = cur.x + dx[dir];
                int ny = cur.y + dy[dir];
                if (!canPass(nx, ny)) continue;
                if (dist[ny][nx] != -1) continue; // 計算済み
                dist[ny][nx] = dist[cur.y][cur.x] + 1;
                Position p;
                p.x = nx;
                p.y = ny;
                todo.push_back(p);
            }
        }
        return dist;
    }

public:
    Simulator() : map_width(0), map_height(0), spawn_percent(0) {
        std::srand(std::time(nullptr)); // 乱数の初期化
    }

    // マップ読み込み
    bool loadMap(const std::string& filename) {
        std::ifstream file(filename);
        if (!file.is_open()) return false;

        std::string line;
        while (std::getline(file, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            grid.push_back(line);
        }
        map_height = grid.size();
        map_width = grid.empty() ? 0 : grid[0].size();
        return true;
    }

    // AGV初期配置（idは追加するたびに自動で 1, 2, 3... と付与）
    void initAGVs(int count) {
        int next_id = 1;
        for (int y = 0; y < map_height; ++y) {
            for (int x = 0; x < map_width; ++x) {
                if (grid[y][x] == 'o' && next_id <= count) {
                    AGV a;
                    a.id = next_id;
                    a.state = AGV_IDLE;
                    a.pos.x = x;
                    a.pos.y = y;
                    a.destination = a.pos; // 待機中は今の場所が目的地
                    a.task_id = -1;
                    a.home = a.pos;
                    agvs.push_back(a);
                    next_id++;
                }
            }
        }
    }

    // その棚のマスに、まだ完了していないタスクがあるか判定
    bool hasActiveTask(int x, int y) {
        for (size_t i = 0; i < tasks.size(); ++i) {
            if (tasks[i].state != TASK_DONE && tasks[i].shelf.x == x && tasks[i].shelf.y == y) {
                return true;
            }
        }
        return false;
    }

    // タスクを1つ、空いている棚のどこかに発生させる（発生できたら true）
    // 未完了のタスクがある棚は選ばない（＝タスク同士が重ならない）
    bool spawnTask(int created_step) {
        // 候補1: 左右どちらかが通路で、未完了のタスクが載っていない棚のマス
        std::vector<Position> candidates;
        for (int y = 0; y < map_height; ++y) {
            for (int x = 0; x < map_width; ++x) {
                if (grid[y][x] == 's' && (canPass(x - 1, y) || canPass(x + 1, y))
                    && !hasActiveTask(x, y)) {
                    Position p;
                    p.x = x;
                    p.y = y;
                    candidates.push_back(p);
                }
            }
        }
        if (candidates.empty()) return false; // 空いている棚がない

        // 候補2: 配送口 'r' のマス
        std::vector<Position> ports;
        for (int y = 0; y < map_height; ++y) {
            for (int x = 0; x < map_width; ++x) {
                if (grid[y][x] == 'r') {
                    Position p;
                    p.x = x;
                    p.y = y;
                    ports.push_back(p);
                }
            }
        }
        if (ports.empty()) return false;

        // 棚をランダムに1つ選ぶ
        Position shelf = candidates[std::rand() % candidates.size()];

        // 受け取りマス：左右のうち通れるほうを選ぶ（両方通れるならランダム）
        std::vector<Position> sides;
        if (canPass(shelf.x - 1, shelf.y)) {
            Position p;
            p.x = shelf.x - 1;
            p.y = shelf.y;
            sides.push_back(p);
        }
        if (canPass(shelf.x + 1, shelf.y)) {
            Position p;
            p.x = shelf.x + 1;
            p.y = shelf.y;
            sides.push_back(p);
        }

        Task t;
        t.id = tasks.size() + 1; // タスクは消さないので、個数+1 がそのまま連番になる
        t.shelf = shelf;
        t.pickup = sides[std::rand() % sides.size()];
        t.destination = ports[std::rand() % ports.size()];
        t.state = TASK_UNASSIGNED;
        t.agv_id = -1;
        t.created_step = created_step;
        tasks.push_back(t);
        return true;
    }

    // 開始時に count 個のタスクをまとめて発生させる
    void generateTasks(int count) {
        for (int n = 0; n < count; ++n) {
            if (!spawnTask(0)) break;
        }
    }

    // 1ステップごとにタスクが発生する確率（%）を設定する
    void setSpawnRate(int percent) {
        spawn_percent = percent;
    }

    // 待機中のAGVに、未割当のタスクを（idの小さい順に）割り当てる
    void assignTasks() {
        for (size_t i = 0; i < agvs.size(); ++i) {
            if (agvs[i].state != AGV_IDLE) continue;

            for (size_t j = 0; j < tasks.size(); ++j) {
                if (tasks[j].state != TASK_UNASSIGNED) continue;

                tasks[j].state = TASK_ASSIGNED;
                tasks[j].agv_id = agvs[i].id;
                agvs[i].task_id = tasks[j].id;
                agvs[i].state = AGV_TO_PICKUP;
                agvs[i].destination = tasks[j].pickup;
                break; // このAGVへの割り当ては1つだけ
            }
        }
    }

    // 全AGVを1歩ずつ動かす（貪欲法＋衝突回避）
    void moveAGVs() {
        // idの小さいAGVから順に動かす。
        // 先に動いたAGVの新しい位置は、後のAGVの判定にすぐ反映される。
        for (size_t i = 0; i < agvs.size(); ++i) {
            bool at_destination = (agvs[i].pos.x == agvs[i].destination.x &&
                                   agvs[i].pos.y == agvs[i].destination.y);

            if (!at_destination) {
                // 目的地までの歩数（他のAGVは考えない）
                std::vector<std::vector<int> > dist = calcDistance(agvs[i].destination);

                int best = dist[agvs[i].pos.y][agvs[i].pos.x]; // 今いるマスの歩数
                int best_dir = -1;                              // -1 は「動かない」

                for (int dir = 0; dir < 4; ++dir) {
                    int nx = agvs[i].pos.x + dx[dir];
                    int ny = agvs[i].pos.y + dy[dir];
                    if (!canPass(nx, ny)) continue;
                    if (dist[ny][nx] == -1) continue;
                    if (isOccupied(nx, ny)) continue;    // 他のAGVがいるマスには入らない
                    if (dist[ny][nx] < best) {         // 今より近く、候補の中で一番近い
                        best = dist[ny][nx];
                        best_dir = dir;
                    }
                }

                if (best_dir != -1) {
                    agvs[i].pos.x += dx[best_dir];
                    agvs[i].pos.y += dy[best_dir];
                }
                // best_dir == -1 のとき（進みたい先に他のAGVがいる等）は、その場で待つ
                // → 互いに相手のマスへ進みたい場合は、お互い動けなくなる（デッドロック）
            }

            // 目的地に着いたときの処理
            if (agvs[i].pos.x == agvs[i].destination.x &&
                agvs[i].pos.y == agvs[i].destination.y) {
                int t = findTaskIndex(agvs[i].task_id);

                if (agvs[i].state == AGV_TO_PICKUP && t != -1) {
                    // 荷物を受け取った → 配送口へ
                    agvs[i].state = AGV_TO_DELIVER;
                    agvs[i].destination = tasks[t].destination;
                } else if (agvs[i].state == AGV_TO_DELIVER && t != -1) {
                    // 配送完了 → タスク完了、待機所へ戻る
                    tasks[t].state = TASK_DONE;
                    agvs[i].state = AGV_IDLE;
                    agvs[i].task_id = -1;
                    agvs[i].destination = agvs[i].home;
                }
            }
        }
    }

    // 1ステップ分の更新（next_step は更新後のステップ番号）
    void update(int next_step) {
        // 一定の確率で、新しいタスクが発生する
        if (std::rand() % 100 < spawn_percent) {
            spawnTask(next_step);
        }
        assignTasks();
        moveAGVs();
    }

    // JSONファイルへ出力する処理
    void exportJSON(const std::string& filename, int steps) {
        std::ofstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Error: Cannot open output file '" << filename << "'" << std::endl;
            return;
        }

        // マップ
        file << "{\n\"map\": [\n";
        for (size_t i = 0; i < grid.size(); ++i) {
            file << "\"" << grid[i] << "\"" << (i == grid.size() - 1 ? "" : ",") << "\n";
        }

        // 各ステップのAGVの様子（シミュレーションしながら書き出す）
        file << "],\n\"history\": [\n";
        for (int step = 0; step < steps; ++step) {
            file << "  [";
            for (size_t i = 0; i < agvs.size(); ++i) {
                file << "{\"id\":" << agvs[i].id
                     << ",\"x\":" << agvs[i].pos.x
                     << ",\"y\":" << agvs[i].pos.y
                     << ",\"state\":" << agvs[i].state
                     << ",\"task\":" << agvs[i].task_id << "}";
                if (i < agvs.size() - 1) file << ",";
            }
            file << "]";
            if (step < steps - 1) file << ",\n";

            update(step + 1); // 次のステップへ
        }

        // タスク一覧（タスクは途中で増えるので、シミュレーション後に書き出す）
        file << "\n],\n\"tasks\": [\n";
        for (size_t i = 0; i < tasks.size(); ++i) {
            file << "  {\"id\":" << tasks[i].id
                 << ",\"created\":" << tasks[i].created_step
                 << ",\"shelf\":[" << tasks[i].shelf.x << "," << tasks[i].shelf.y << "]"
                 << ",\"pickup\":[" << tasks[i].pickup.x << "," << tasks[i].pickup.y << "]"
                 << ",\"destination\":[" << tasks[i].destination.x << "," << tasks[i].destination.y << "]}";
            if (i < tasks.size() - 1) file << ",";
            file << "\n";
        }
        file << "]\n}\n";
        file.close();

        // 結果のまとめを表示
        int done = 0;
        for (size_t i = 0; i < tasks.size(); ++i) {
            if (tasks[i].state == TASK_DONE) done++;
        }
        std::cout << "Simulation Complete: Exported to " << filename << std::endl;
        std::cout << "Tasks done: " << done << " / " << tasks.size() << " (generated)" << std::endl;
    }
};

int main() {
    Simulator sim;

    // マップ読み込み
    if (!sim.loadMap("map/map_ex.txt")) {
        std::cerr << "Error: Cannot open map file 'map_ex.txt'" << std::endl;
        return 1;
    }

    sim.initAGVs(32);       // AGVを32台配置
    sim.generateTasks(25);   // 開始時に25個のタスクを発生
    sim.setSpawnRate(20);   // 以降、1ステップごとに20%の確率で新しいタスクが発生

    // output/history.json に200ステップ分のデータを出力
    sim.exportJSON("output/history.json", 200);

    return 0;
}