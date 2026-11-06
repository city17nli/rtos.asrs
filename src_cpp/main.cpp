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

// AGVクラス
class AGV {
public:
    int id;
    Position pos;
    AGV(int id, int x, int y) : id(id), pos{x, y} {}
};

// シミュレータ全体を管理するクラス
class Simulator {
private:
    std::vector<std::string> grid;
    std::vector<AGV> agvs;
    int map_width, map_height;

public:
    Simulator() : map_width(0), map_height(0) { 
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

    // AGV初期配置
    void initAGVs(int count) {
        for (int y = 0; y < map_height; ++y) {
            for (int x = 0; x < map_width; ++x) {
                if (grid[y][x] == 'o' && agvs.size() < count) {
                    agvs.push_back(AGV(agvs.size() + 1, x, y));
                }
            }
        }
    }

    // ランダムに1マス移動（壁 'w' 以外なら進む）
    void randomWalk() {
        int dx[] = {0, 0, -1, 1}; // 上, 下, 左, 右
        int dy[] = {-1, 1, 0, 0};
        for (auto& agv : agvs) {
            int dir = std::rand() % 4;
            int nx = agv.pos.x + dx[dir];
            int ny = agv.pos.y + dy[dir];
            if (nx >= 0 && nx < map_width && ny >= 0 && ny < map_height) {
                if (grid[ny][nx] != 'w' && grid[ny][nx] != 's') { // 壁や他のAGVがいない場合
                    agv.pos.x = nx;
                    agv.pos.y = ny;
                }
            }
        }
    }

    // JSONファイルへ出力する処理
    void exportJSON(const std::string& filename, int steps) {
        std::ofstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Error: Cannot open output file '" << filename << "'" << std::endl;
            return;
        }

        // JSONの書き出し開始
        file << "{\n\"map\": [\n";
        for (size_t i = 0; i < grid.size(); ++i) {
            file << "\"" << grid[i] << "\"" << (i == grid.size() - 1 ? "" : ",") << "\n";
        }
        file << "],\n\"history\": [\n";

        // 指定ステップ数だけシミュレーションを回して座標を記録
        for (int step = 0; step < steps; ++step) {
            file << "  [";
            for (size_t i = 0; i < agvs.size(); ++i) {
                file << "{\"id\":" << agvs[i].id << ",\"x\":" << agvs[i].pos.x << ",\"y\":" << agvs[i].pos.y << "}";
                if (i < agvs.size() - 1) file << ",";
            }
            file << "]";
            if (step < steps - 1) file << ",\n";
            
            randomWalk(); // 次のステップへ移動
        }
        
        file << "\n]\n}\n";
        file.close();
        std::cout << "Simulation Complete: Exported to " << filename << std::endl;
    }
};

int main() {
    Simulator sim;
    
    // マップ読み込み
    if (!sim.loadMap("map/map_ex.txt")) {
        std::cerr << "Error: Cannot open map file 'map_ex.txt'" << std::endl;
        return 1;
    }
    
    sim.initAGVs(5); // AGVを5台配置
    
    // output/history.json に30ステップ分のデータを出力
    sim.exportJSON("output/history.json", 30); 
    
    return 0;
}