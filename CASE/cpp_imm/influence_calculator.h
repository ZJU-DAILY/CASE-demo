#ifndef INFLUENCE_CALCULATOR_H
#define INFLUENCE_CALCULATOR_H

#include "community.h"
// 声明核心计算函数，它接收一个API请求结构体，并返回一个API结果结构体
ApiResult run_influence_maximization(const ApiRequest& request);

ApiMinResult run_influence_minimization(const ApiRequest& request);

ApiFinalInfluence get_final_influence(
    const string& dataset_id, 
    const string& propagation_model, 
    const string& probability_model, 
    const vector<int>& initial_nodes,
    const vector<int>& blocking_nodes // 【新增】
);

// 【新增】声明用于获取概率波动画数据的函数
ApiSimulationResult get_probability_animation(
    const string& dataset_id, 
    const string& propagation_model, 
    const string& probability_model, 
    const vector<int>& initial_nodes,
    const vector<int>& blocking_nodes // 新增阻塞节点参数
);

// 【【【新增】】】声明可以“从零开始”的 (k,l)-core 社区分析函数
ApiCommunityResult run_kl_core_analysis_from_scratch(
    const string& dataset_id,
    const string& propagation_model,
    const string& probability_model,
    int k_core,
    int l_core,
    int seed_budget,
    const string& seed_generation_mode,
    const vector<int>& manual_seeds // 允许用户手动输入种子
);




// 【【【新增】】】声明可以“从零开始”的 k-core 社区分析函数
ApiCommunityResult run_k_core_analysis_from_scratch(
    const string& dataset_id,
    const string& propagation_model,
    const string& probability_model,
    int k_core,
    int seed_budget,
    const string& seed_generation_mode,
    const vector<int>& manual_seeds
);


// 【【【新增】】】声明可以“从零开始”的 k-truss 社区分析函数
ApiCommunityResult run_k_truss_analysis_from_scratch(
    const string& dataset_id,
    const string& propagation_model,
    const string& probability_model,
    int k_truss,
    int seed_budget,
    const string& seed_generation_mode,
    const vector<int>& manual_seeds
);


// 在 influence_calculator.h 文件中，与其他函数声明放在一起
// 【新增】声明用于获取阻塞动画数据的函数
// 【修改】此函数的声明
ApiSimulationResult get_blocking_animation(
    const string& dataset_id,
    const string& propagation_model,
    const string& probability_model,
    const vector<int>& initial_nodes,
    const vector<int>& blocking_nodes
);

// 【添加】将这个新函数声明添加到 influence_calculator.h 中
ApiCriticalPathResult find_critical_paths(
    const string& result_id,
    const string& dataset_id,
    const string& propagation_model,
    const string& probability_model,
    const vector<int>& initial_nodes
);

#endif // INFLUENCE_CALCULATOR_H
