#ifndef API_STRUCTURES_H
#define API_STRUCTURES_H

#include <string>
#include <vector>
#include <stdexcept>

// 使用 `using` 来避免反复写 std::
using std::string;
using std::vector;

struct Edge {
    int source;
    int target;
};

// --- API 输入结构体 ---
struct InfluenceParams {
    string propagation_model; 
    string probability_model; 
    int budget;                     
    vector<int> seed_nodes; 
    int neg_num;
    string seed_generation_mode; // <--- 【新增】用于控制种子生成模式 ("IMM" 或 "RANDOM")
};

struct ApiRequest {
    string dataset_id;
    string mode; 
    InfluenceParams params;
};


// --- 最大化返回体 ---
struct SeedNodeResult {
    int id;
    double priority;
};

struct FinalInfluenceResult {
    int count;
    double ratio;
};

struct ApiResult {
    string result_id;
    vector<SeedNodeResult> seed_nodes;
    FinalInfluenceResult final_influence;
    string message;
    vector<Edge> main_propagation_paths;
};


// --- 最小化返回体 ---
struct BlockingNodeResult {
    int id;
    double priority;
};

struct ApiMinResult {
    string original_result_id;
    string blocked_result_id;
    vector<BlockingNodeResult> blocking_nodes;
    vector<int> seed_nodes;
    FinalInfluenceResult influence_before;
    FinalInfluenceResult influence_after;
    double reduction_ratio;
    vector<Edge> cut_off_paths; 
    string message;
};


// --- 通用节点状态与最终结果返回体 ---

// 该结构体已存在，可用于最终结果和动画步骤，表示单个节点的状态
struct NodeState {
    int id;
    string state;
    double probability;
};

// 该结构体已存在，用于封装 get_final_influence 的返回结果
struct ApiFinalInfluence {
    string result_id;
    vector<NodeState> final_states;
    double total_influence; // 最终总影响力（所有概率之和）
};


// --- 【新增】为“概率波”动画接口新增的结构体 ---

// SimulationStep: 代表一次模拟迭代（一个时间步）结束后的网络状态快照
struct SimulationStep {
    int step;
    vector<int> newly_activated_nodes;
    vector<int> newly_recovered_nodes; // <--- 新增这一行
    vector<NodeState> node_states;
};

// ApiSimulationResult: 包含整个“概率波”动画过程的完整结果
struct ApiSimulationResult {
    string result_id;
    int total_steps;                   // 动画收敛所需的总步数
    vector<SimulationStep> simulation_steps;
};

// CommunityResult: 代表找到的社区的核心数据
struct CommunityResult {
    vector<int> node_ids;
    double average_influence_prob;
    int node_count;
};

// ApiCommunityResult: 包含社区分析的完整API返回结果
struct ApiCommunityResult {
    string result_id;
    CommunityResult community;
    string message;
    vector<NodeState> final_states;
    vector<int> seed_nodes;
};

// 【添加】将这些新结构体添加到 api_structures.h 文件末尾

// 代表一条路径和其得分（例如，深度）
struct CriticalPath {
    vector<int> nodes; // 路径上的节点ID序列
    double score;      // 路径的得分，这里是深度
    string type;       // 路径类型, e.g., "deepest"
};

// 封装关键路径分析的完整API返回体
struct ApiCriticalPathResult {
    string result_id;
    vector<CriticalPath> critical_paths;
    string message;
};

#endif // API_STRUCTURES_H