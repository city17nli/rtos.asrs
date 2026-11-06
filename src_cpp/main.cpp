#include <iostream>
#include <fstream>
#include <vector>
#include <string>

// 2次元座標を扱う構造体
struct Position {
    int x;
    int y;
};

// AGVクラス
class AGV {
public:
    int id;
    Position pos;

    // コンストラクタ（生成時にIDと初期座標をセット）
    AGV(int id, int x, int y) : id(id), pos{x, y} {}
};

// シミュレータ全体を管理するクラス
class Simulator {
private:
    std::vector<std::string> grid; // マップデータ
    std::vector<AGV> agvs;         // AGVのリスト
    int map_width;
    int map_height;

public:
    Simulator() : map_width(0), map_height(0) {}

    // 1. マップファイルの読み込み
    bool loadMap(const std::string& filename) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            std::cerr << "エラー: マップファイル '" << filename << "' が開けません。" << std::endl;
            return false;
        }

        std::string line;
        while (std::getline(file, line)) {
            // Windowsの改行コード(CRLF)対策: 末尾の '\r' を削除
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            grid.push_back(line);
        }
        file.close();

        if (grid.empty()) return false;

        map_height = grid.size();
        map_width = grid[0].size();
        return true;
    }

    // 2. AGVの初期配置
    void initAGVs(int agv_count) {
        std::vector<Position> spawn_points;

        // マップ全体を走査して生成地 'o' の座標をリストアップ
        // (yが縦方向, xが横方向)
        for (int y = 0; y < map_height; ++y) {
            for (int x = 0; x < map_width; ++x) {
                if (grid[y][x] == 'o') {
                    spawn_points.push_back({x, y});
                }
            }
        }

        // 指定された台数（5台）だけAGVを生成してリストに追加
        for (int i = 0; i < agv_count && i < spawn_points.size(); ++i) {
            // IDは1から割り当て
            agvs.push_back(AGV(i + 1, spawn_points[i].x, spawn_points[i].y));
        }
    }

    // 3. 現在の情報をコンソールに出力
    void printState() {
        std::cout << "=== Map Data (" << map_width << "x" << map_height << ") ===" << std::endl;
        for (const auto& row : grid) {
            std::cout << row << std::endl;
        }

        std::cout << "\n=== AGV Initial Positions ===" << std::endl;
        for (const auto& agv : agvs) {
            std::cout << "AGV ID: " << agv.id 
                      << " | Position: (x: " << agv.pos.x << ", y: " << agv.pos.y << ")" << std::endl;
        }
    }
};

int main() {
    Simulator sim;

    // rtos.asrs フォルダ直下にある map_ex.txt を読み込む想定
    if (!sim.loadMap("../map_ex.txt")) { 
        // 実行場所によっては "map_ex.txt" に変更してください
        return 1;
    }

    // AGVを5台初期配置
    sim.initAGVs(5);

    // 状態をコンソールに出力
    sim.printState();

    return 0;
}