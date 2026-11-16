#include <fstream>
#include <iostream>
#include <queue>
#include <string>
#include <vector>

using namespace std;

const string MAP_PATH = "map/map_demo.txt";
const string OUT_PATH = "output/sim_greedy.json";
const int MAX_TICKS = 200;  // 安全のための上限

enum State { MOVING, RETURNING, WAITING, DONE };
const char* STATE_NAME[] = {"moving", "returning", "waiting", "done"};

struct Position {
    int x, y;
    bool operator==(const Position& o) const { return x == o.x && y == o.y; }
};

struct AGV {
    int id;
    Position pos;    // 現在位置
    Position start;  // スタート地点(o)
    Position goal;   // 目標(r)
    bool returning = false;  // rに到着したらtrue(以降はstartへ向かう)
    State state = MOVING;
    vector<vector<int>> dist;  // 今の目標までの距離場

    Position target() const { return returning ? start : goal; }
};

// 4近傍(優先順位 = 同距離のときの選択順): 下, 左, 右, 上
const int dx[4] = {0, -1, 1, 0};
const int dy[4] = {1, 0, 0, -1};

vector<string> grid;
int width, height;

bool load_map(const string& path) {
    ifstream f(path);
    if (!f) return false;
    string line;
    while (getline(f, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (!line.empty()) grid.push_back(line);
    }
    height = grid.size();
    width = grid[0].size();
    return true;
}

bool passable(int x, int y) {
    if (x < 0 || y < 0 || x >= width || y >= height) return false;
    return grid[y][x] == '.' || grid[y][x] == 'o' || grid[y][x] == 'r';  // 壁(w)・棚(s)以外
}

// 目標からのBFS距離場: 壁・棚を避けた実際の最短歩数(他AGVは無視)。未訪問は-1
vector<vector<int>> bfs(Position target) {
    vector<vector<int>> d(height, vector<int>(width, -1));
    queue<Position> q;
    d[target.y][target.x] = 0;
    q.push(target);
    while (!q.empty()) {
        Position p = q.front();
        q.pop();
        for (int k = 0; k < 4; k++) {
            int nx = p.x + dx[k], ny = p.y + dy[k];
            if (passable(nx, ny) && d[ny][nx] == -1) {
                d[ny][nx] = d[p.y][p.x] + 1;
                q.push({nx, ny});
            }
        }
    }
    return d;
}

// oに1台ずつAGVを置く。目標rは交互に割り当て(0,1,0,1,...)
vector<AGV> create_agvs() {
    vector<Position> spawns, goals;
    for (int y = 0; y < height; y++)
        for (int x = 0; x < width; x++) {
            if (grid[y][x] == 'o') spawns.push_back({x, y});
            if (grid[y][x] == 'r') goals.push_back({x, y});
        }
    vector<AGV> agvs;
    for (size_t i = 0; i < spawns.size(); i++) {
        AGV a;
        a.id = i;
        a.pos = a.start = spawns[i];
        a.goal = goals[i % goals.size()];
        a.dist = bfs(a.goal);
        agvs.push_back(a);
    }
    return agvs;
}

// 貪欲法: 目標に近づく隣接マスのうち、空いている最初のマスを next に入れる。なければ false
bool greedy_next(const AGV& a, const vector<vector<int>>& occ, Position& next) {
    int cur = a.dist[a.pos.y][a.pos.x];
    for (int k = 0; k < 4; k++) {
        int nx = a.pos.x + dx[k], ny = a.pos.y + dy[k];
        if (!passable(nx, ny)) continue;
        if (a.dist[ny][nx] >= cur) continue;  // 近づかないマスは使わない
        if (occ[ny][nx] != -1) continue;      // 誰かいたら使えない
        next = {nx, ny};
        return true;
    }
    return false;
}

// 1tick進める(ID順に処理)。動けたAGVの数を返す
int step(vector<AGV>& agvs) {
    vector<vector<int>> occ(height, vector<int>(width, -1));  // 占有マップ
    for (auto& a : agvs) occ[a.pos.y][a.pos.x] = a.id;

    int moved = 0;
    for (auto& a : agvs) {
        if (a.state == DONE) continue;

        Position next{};
        if (!greedy_next(a, occ, next)) { a.state = WAITING; continue; }

        occ[a.pos.y][a.pos.x] = -1;
        a.pos = next;
        occ[next.y][next.x] = a.id;
        moved++;

        if (!(a.pos == a.target())) {
            a.state = a.returning ? RETURNING : MOVING;
        } else if (a.returning) {
            a.state = DONE;  // スタートに戻った
        } else {
            a.returning = true;  // rに到着 -> 次のtickからスタートへ戻る
            a.state = RETURNING;
            a.dist = bfs(a.start);
        }
    }
    return moved;
}

bool all_done(const vector<AGV>& agvs) {
    for (auto& a : agvs)
        if (a.state != DONE) return false;
    return true;
}

void write_frame(ofstream& out, int t, const vector<AGV>& agvs) {
    out << "    {\"t\": " << t << ", \"agvs\": [";
    for (size_t i = 0; i < agvs.size(); i++) {
        const AGV& a = agvs[i];
        out << (i ? ", " : "") << "{\"id\": " << a.id << ", \"x\": " << a.pos.x
            << ", \"y\": " << a.pos.y << ", \"state\": \"" << STATE_NAME[a.state] << "\"}";
    }
    out << "]}";
}

int main() {
    if (!load_map(MAP_PATH)) { cerr << "map not found: " << MAP_PATH << endl; return 1; }
    vector<AGV> agvs = create_agvs();

    ofstream out(OUT_PATH);
    if (!out) { cerr << "cannot write: " << OUT_PATH << endl; return 1; }
    out << "{\n  \"map\": [";
    for (int y = 0; y < height; y++) out << (y ? ", " : "") << "\"" << grid[y] << "\"";
    out << "],\n  \"width\": " << width << ",\n  \"height\": " << height << ",\n  \"frames\": [\n";
    write_frame(out, 0, agvs);

    int t = 0;
    bool finished = false;
    while (t < MAX_TICKS) {
        t++;
        int moved = step(agvs);
        out << ",\n";
        write_frame(out, t, agvs);
        finished = all_done(agvs);
        if (finished || moved == 0) break;  // 全員戻った / 誰も動けない(以後も変化しない)
    }

    out << "\n  ],\n  \"result\": {\"finished\": " << (finished ? "true" : "false")
        << ", \"ticks\": " << t << "}\n}\n";

    cout << (finished ? "all AGVs returned" : "not finished") << " at tick " << t << endl;
    return 0;
}