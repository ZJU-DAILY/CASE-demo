#include "influence_calculator.h"
#include "imm.h" // 【修正】添加缺失的头文件
#include <stdexcept>
#include <string>
#include <set>
#include <uuid/uuid.h>
#include "imm.h"

// 辅助函数：将字符串格式的传播模型转换为全局的InfluModel枚举类型
InfluModel model_str_to_enum(const std::string& model_str) {
    if (model_str == "IC") return IC;
    if (model_str == "LT") return LT;
    throw std::invalid_argument("Unsupported propagation model provided: " + model_str);
}

// 辅助函数：生成一个唯一的UUID字符串，用作结果ID
std::string generate_uuid() {
    uuid_t uuid;
    uuid_generate_random(uuid);
    char uuid_str[37];
    uuid_unparse_lower(uuid, uuid_str);
    return std::string(uuid_str);
}

// in influence_calculator.cpp

// 【用这个完整版本替换现有的 run_influence_maximization 函数】
ApiResult run_influence_maximization(const ApiRequest& request) {
    if (request.mode != "maximization") {
        throw std::runtime_error("This function is for maximization mode only.");
    }

    Argument arg;
    arg.k = request.params.budget;
    arg.model = request.params.propagation_model;
    arg.epsilon = 0.1;
    
    std::string graph_filepath = "./" + request.dataset_id + "_subset_1000.txt";
    InfGraph g(graph_filepath);
    g.setInfuModel(model_str_to_enum(arg.model));
    g.setActiveProbabilityModel(request.params.probability_model);

    // 步骤 1: 使用IMM算法高效地【寻找】最优种子节点集合 (这部分保持不变)
    Imm::InfluenceMaximize(g, arg);

    ApiResult result;
    result.result_id = generate_uuid();
    
    vector<int> seed_node_ids;
    for (int seed_node_id : g.result_node_set) {
        result.seed_nodes.push_back({seed_node_id, 0.0});
        seed_node_ids.push_back(seed_node_id);
    }
    
    // ================= 【核心修改开始】 =================
    // 步骤 2: 【废弃】旧的估算方法
    // double influence_spread_estimate = g.InfluenceHyperGraph(); 

    // 步骤 3: 【采用】与可视化一致的精确模拟法来【计算】影响力
    const int NUM_SIMULATIONS_FOR_ACCURACY = 10000;
    const double ACTIVATION_THRESHOLD = 0.5;

    vector<double> final_probs = g.calculate_final_probabilities(seed_node_ids, NUM_SIMULATIONS_FOR_ACCURACY, {});
    int accurate_influence_count = 0;
    for (double prob : final_probs) {
        if (prob >= ACTIVATION_THRESHOLD) {
            accurate_influence_count++;
        }
    }

    // 使用精确计算出的数值填充返回结果
    result.final_influence.count = accurate_influence_count;
    result.final_influence.ratio = (g.n > 0) ? (static_cast<double>(accurate_influence_count) / g.n) : 0.0;
    // ================= 【核心修改结束】 =================


    // 步骤 4: 查找主要传播路径 (这部分不变)
    result.main_propagation_paths = g.find_main_propagation_paths(seed_node_ids);

    // 步骤 5: 更新返回消息，现在不再是 "estimated"
    result.message = "Influence maximization complete. Using propagation model '" + arg.model 
                   + "' and probability model '" + request.params.probability_model
                   + "'. Selected " + std::to_string(arg.k) 
                   + " seed nodes, resulting in a simulated influence of " + std::to_string(result.final_influence.count) + " nodes.";
    return result;
}

ApiMinResult run_influence_minimization(const ApiRequest& request) {
    if (request.mode != "minimization") {
        throw std::runtime_error("This function is for minimization mode only.");
    }

    // 1. 加载图并设置模型 (这部分不变)
    std::string graph_filepath = "./" + request.dataset_id + "_subset_1000.txt";
    InfGraph g(graph_filepath);
    g.setInfuModel(model_str_to_enum(request.params.propagation_model));
    g.setActiveProbabilityModel(request.params.probability_model);

    ApiMinResult result;
    
    // ================= 【核心修改开始】 =================
    vector<int> negative_seeds;
    if (request.params.seed_nodes.empty()) {
        int num_seeds_to_generate = max(request.params.neg_num, 1);
        
        // 根据新的标识符字段来决定生成策略
        if (request.params.seed_generation_mode == "IMM") {
            // --- IMM 模式 ---
            // 调用影响力最大化算法来寻找 "最具破坏力" 的种子
            std::cout << "[INFO] Using IMM to generate " << num_seeds_to_generate << " negative seeds..." << std::endl;
            Argument arg_for_seeds;
            arg_for_seeds.k = num_seeds_to_generate;
            arg_for_seeds.model = request.params.propagation_model; // 与主任务模型保持一致
            arg_for_seeds.epsilon = 0.1; // 使用标准的epsilon值
            
            // 运行IMM算法，结果会保存在 g.result_node_set 中
            Imm::InfluenceMaximize(g, arg_for_seeds); 
            negative_seeds = g.result_node_set;

        } else { // 默认为 "RANDOM" 模式
            // --- 随机模式 ---
            std::cout << "[INFO] Using RANDOM method to generate " << num_seeds_to_generate << " negative seeds..." << std::endl;
            negative_seeds = g.generate_random_seeds(num_seeds_to_generate);
        }
    } else {
        // 如果用户手动提供了种子，则直接使用
        negative_seeds = request.params.seed_nodes;
    }
    // ================= 【核心修改结束】 =================

    result.seed_nodes = negative_seeds;
    
    // ================= 【核心修改开始】 =================
    // 我们将使用更精确的蒙特卡洛模拟来计算影响力数值，以确保与可视化结果一致。
    const int NUM_SIMULATIONS_FOR_ACCURACY = 10000; // 定义一个用于精确计数的模拟次数
    const double ACTIVATION_THRESHOLD = 0.5;      // 定义节点被视为“激活”的概率阈值

    // 2. 估算阻塞前影响力 (使用新的精确模拟法)
    vector<double> probs_before = g.calculate_final_probabilities(negative_seeds, NUM_SIMULATIONS_FOR_ACCURACY, {});
    int influence_count_before = 0;
    for (double prob : probs_before) {
        if (prob >= ACTIVATION_THRESHOLD) {
            influence_count_before++;
        }
    }
    result.influence_before.count = influence_count_before;
    result.influence_before.ratio = (g.n > 0) ? (static_cast<double>(influence_count_before) / g.n) : 0.0;

    int budget = request.params.budget;
    
    // 3. 构建超图以【寻找】阻塞节点 (这部分仍然使用快速的IMM/RR集方法，因为它的目的是“寻找”最优集合，速度是关键)
    int64_t R = 100000; 
    g.init_hyper_graph();
    g.build_hyper_graph_for_minimization(R, negative_seeds);

    // 4. 选择阻塞节点 (这部分不变)
    g.build_blocking_set(budget, negative_seeds);
    vector<int> blocking_nodes = g.result_node_set;

    // 5. 估算阻塞后影响力 (同样使用新的精确模拟法)
    vector<double> probs_after = g.calculate_final_probabilities(negative_seeds, NUM_SIMULATIONS_FOR_ACCURACY, blocking_nodes);
    int influence_count_after = 0;
    for (double prob : probs_after) {
        if (prob >= ACTIVATION_THRESHOLD) {
            influence_count_after++;
        }
    }
    result.influence_after.count = influence_count_after;
    result.influence_after.ratio = (g.n > 0) ? (static_cast<double>(influence_count_after) / g.n) : 0.0;
    
    result.cut_off_paths = g.find_cut_off_edges(negative_seeds, blocking_nodes);
    
    // 7. 填充所有返回字段 (这部分不变)
    result.original_result_id = generate_uuid();
    result.blocked_result_id = generate_uuid();
    
    for (int node_id : blocking_nodes) {
        result.blocking_nodes.push_back({node_id, 0.0});
    }

    if (influence_count_before > 0) {
        result.reduction_ratio = static_cast<double>(influence_count_before - influence_count_after) / influence_count_before;
    } else {
        result.reduction_ratio = 0.0;
    }
    
    result.message = "Influence minimization complete. Selected " + std::to_string(budget)
                   + " blocking nodes, reducing influence by approximately " + std::to_string(result.reduction_ratio * 100) 
                   + "%. Found " + std::to_string(result.cut_off_paths.size()) + " sample cut-off paths.";

    return result;
}
// --- 【新增】为MICS接口提供数据 ---
ApiFinalInfluence get_final_influence(const string& dataset_id, const string& propagation_model, const string& probability_model, const vector<int>& initial_nodes, const vector<int>& blocking_nodes) {
    
    // 1. 加载图并设置模型 (与之前的逻辑相同)
    std::string graph_filepath = "./" + dataset_id + "_subset_1000.txt";
    InfGraph g(graph_filepath);
    g.setInfuModel(model_str_to_enum(propagation_model));
    g.setActiveProbabilityModel(probability_model);

    // 2. 调用【新】的、只返回最终概率的函数
    vector<double> final_probs = g.calculate_final_probabilities(
        initial_nodes,
        10000, // 使用一个较高的模拟次数以保证精度
        blocking_nodes // 【传入】
    );
    
    // 3. 将结果包装成 FinalInfluenceResult 结构体
    ApiFinalInfluence result;
    result.result_id = "final_influence_result"; // 可以生成一个唯一ID
    
    double total_prob_sum = 0.0;
    double threshold = 0.5; // 定义激活阈值
    
    for(int i = 0; i < final_probs.size(); ++i) {
        if (final_probs[i] > 1e-6) { // 只返回有影响的节点以节省空间
            string state = (final_probs[i] >= threshold) ? "active" : "inactive";
            result.final_states.push_back({i, state, final_probs[i]});
            total_prob_sum += final_probs[i];
        }
    }
    result.total_influence = total_prob_sum;
    
    return result;
}

ApiSimulationResult get_probability_animation(
    const string& dataset_id, 
    const string& propagation_model, 
    const string& probability_model, 
    const vector<int>& initial_nodes,
    const vector<int>& blocking_nodes
) {
    // 1. 加载图并设置模型
    std::string graph_filepath = "./" + dataset_id + "_subset_1000.txt";
    InfGraph g(graph_filepath);
    g.setInfuModel(model_str_to_enum(propagation_model));
    g.setActiveProbabilityModel(probability_model);

    // 2. 调用 InfGraph 中我们为概率波动画设计的核心函数
    ApiSimulationResult result = g.run_probability_simulation(initial_nodes, blocking_nodes);
    result.result_id = generate_uuid(); // 为这次动画生成一个ID
    
    return result;
}

ApiCommunityResult run_k_core_analysis_from_scratch(
    const string& dataset_id,
    const string& propagation_model,
    const string& probability_model,
    int k_core,
    int seed_budget,
    const string& seed_generation_mode,
    const vector<int>& manual_seeds
) {
    // 1. 加载图
    std::string graph_filepath = "./" + dataset_id + "_subset_1000.txt";
    InfGraph g(graph_filepath);
    g.setInfuModel(model_str_to_enum(propagation_model));
    g.setActiveProbabilityModel(probability_model);

    // 2. 根据模式生成种子节点
    vector<int> query_nodes;
    if (!manual_seeds.empty()) {
        query_nodes = manual_seeds;
    } else {
        if (seed_generation_mode == "IMM") {
            Argument arg_for_seeds;
            arg_for_seeds.k = seed_budget;
            arg_for_seeds.model = propagation_model;
            arg_for_seeds.epsilon = 0.1;
            Imm::InfluenceMaximize(g, arg_for_seeds);
            query_nodes = g.result_node_set;
        } else { // "RANDOM"
            query_nodes = g.generate_random_seeds(seed_budget);
        }
    }

    // 3. 基于生成的种子计算影响力最终状态
    ApiFinalInfluence influence_result = get_final_influence(dataset_id, propagation_model, probability_model, query_nodes, {});
     if (influence_result.final_states.empty()) {
        ApiCommunityResult result;
        result.result_id = "from_scratch_result";
        result.message = "Generated seeds did not result in any influence, cannot perform community analysis.";
        return result;
    }

    // 4. 调用核心的社区发现算法
    CommunityResult community = CommunitySearcher::find_k_core_community(k_core, influence_result.final_states, g, query_nodes);

    // 5. 封装返回结果
    ApiCommunityResult result;
    result.result_id = "from_scratch_result";
    result.community = community;
    result.final_states = influence_result.final_states; // 【核心修改】将影响力状态存入结果
    result.seed_nodes = query_nodes;
    if (community.node_count > 0) {
        result.message = "Found an undirected community that satisfies the " + std::to_string(k_core) + "-core condition.";
    } else {
        result.message = "No undirected community satisfying the " + std::to_string(k_core) + "-core condition was found for the generated seeds.";
    }
    return result;
}

ApiCommunityResult run_kl_core_analysis_from_scratch(
    const string& dataset_id,
    const string& propagation_model,
    const string& probability_model,
    int k_core,
    int l_core,
    int seed_budget,
    const string& seed_generation_mode,
    const vector<int>& manual_seeds
) {
    // 1. 加载图
    std::string graph_filepath = "./" + dataset_id + "_subset_1000.txt";
    InfGraph g(graph_filepath);
    g.setInfuModel(model_str_to_enum(propagation_model));
    g.setActiveProbabilityModel(probability_model);

    // 2. 根据模式生成种子节点
    vector<int> query_nodes;
    if (!manual_seeds.empty()) {
        query_nodes = manual_seeds;
    } else {
        if (seed_generation_mode == "IMM") {
            Argument arg_for_seeds;
            arg_for_seeds.k = seed_budget;
            arg_for_seeds.model = propagation_model;
            arg_for_seeds.epsilon = 0.1;
            Imm::InfluenceMaximize(g, arg_for_seeds);
            query_nodes = g.result_node_set;
        } else { // "RANDOM"
            query_nodes = g.generate_random_seeds(seed_budget);
        }
    }

    // 3. 基于生成的种子计算影响力最终状态
    ApiFinalInfluence influence_result = get_final_influence(dataset_id, propagation_model, probability_model, query_nodes, {});
    if (influence_result.final_states.empty()) {
        ApiCommunityResult result;
        result.result_id = "from_scratch_result";
        result.message = "Generated seeds did not result in any influence, cannot perform community analysis.";
        return result;
    }

    // 4. 调用核心的社区发现算法
    CommunityResult community = CommunitySearcher::find_most_influenced_community_local(k_core, l_core, influence_result.final_states, g, query_nodes);

    // 5. 封装返回结果
    ApiCommunityResult result;
    result.result_id = "from_scratch_result";
    result.community = community;
    result.final_states = influence_result.final_states; // 【核心修改】将影响力状态存入结果
    result.seed_nodes = query_nodes;
    if (community.node_count > 0) {
        result.message = "Found a community that satisfies the (" + std::to_string(k_core) + "," + std::to_string(l_core) +
                         ")-core condition with an average influence probability of " + std::to_string(community.average_influence_prob) + ".";
    } else {
        result.message = "No community satisfying the (" + std::to_string(k_core) + "," + std::to_string(l_core) + ")-core condition was found for the generated seeds.";
    }

    return result;
}

// 【【【新增】】】实现可以“从零开始”的 k-truss 社区分析函数
ApiCommunityResult run_k_truss_analysis_from_scratch(
    const string& dataset_id,
    const string& propagation_model,
    const string& probability_model,
    int k_truss,
    int seed_budget,
    const string& seed_generation_mode,
    const vector<int>& manual_seeds
) {
    // 1. 加载图
    std::string graph_filepath = "./" + dataset_id + "_subset_1000.txt";
    InfGraph g(graph_filepath);
    g.setInfuModel(model_str_to_enum(propagation_model));
    g.setActiveProbabilityModel(probability_model);

    // 2. 根据模式生成种子节点
    vector<int> query_nodes;
    if (!manual_seeds.empty()) {
        query_nodes = manual_seeds;
    } else {
        if (seed_generation_mode == "IMM") {
            Argument arg_for_seeds;
            arg_for_seeds.k = seed_budget;
            arg_for_seeds.model = propagation_model;
            arg_for_seeds.epsilon = 0.1;
            Imm::InfluenceMaximize(g, arg_for_seeds);
            query_nodes = g.result_node_set;
        } else { // "RANDOM"
            query_nodes = g.generate_random_seeds(seed_budget);
        }
    }

    ApiFinalInfluence influence_result = get_final_influence(dataset_id, propagation_model, probability_model, query_nodes, {});

    // 3. 基于生成的种子计算影响力最终状态
    if (influence_result.final_states.empty()) {
        ApiCommunityResult result;
        result.result_id = "from_scratch_result";
        result.message = "Generated seeds did not result in any influence, cannot perform community analysis.";
        
        return result;
    }

    // 4. 调用核心的社区发现算法
    CommunityResult community = CommunitySearcher::find_k_truss_community(k_truss, influence_result.final_states, g, query_nodes);

    // 5. 封装返回结果
    ApiCommunityResult result;
    result.result_id = "from_scratch_result";
    result.community = community;
    result.final_states = influence_result.final_states; // 【核心修改】将影响力状态存入结果
    result.seed_nodes = query_nodes;
    if (community.node_count > 0) {
        result.message = "Found an undirected community that satisfies the " + std::to_string(k_truss) + "-truss condition.";
    } else {
        result.message = "No undirected community satisfying the " + std::to_string(k_truss) + "-truss condition was found for the generated seeds.";
    }
    return result;
}
// 【最终修正】替换 influence_calculator.cpp 中的 get_blocking_animation 函数
ApiSimulationResult get_blocking_animation(
    const string& dataset_id,
    const string& propagation_model,
    const string& probability_model,
    const vector<int>& initial_nodes,
    const vector<int>& blocking_nodes
) {
    ApiSimulationResult result;
    result.result_id = generate_uuid();

    // 1. 加载图并设置模型
    std::string graph_filepath = "./" + dataset_id + "_subset_1000.txt";
    InfGraph g(graph_filepath);
    g.setInfuModel(model_str_to_enum(propagation_model));
    g.setActiveProbabilityModel(probability_model);

    // 2. Step 0: 计算完全阻塞前的状态
    SimulationStep step0;
    step0.step = 0;
    vector<double> probs_before = g.calculate_final_probabilities(initial_nodes, 10000, {});
    set<int> previously_active_ids;
    for(size_t i = 0; i < probs_before.size(); ++i) {
        if (probs_before[i] > 0.5) {
            step0.node_states.push_back({(int)i, "active", probs_before[i]});
            previously_active_ids.insert(i);
        } else if (probs_before[i] > 1e-6) {
             step0.node_states.push_back({(int)i, "inactive", probs_before[i]});
        }
    }
    result.simulation_steps.push_back(step0);

    // --- 【核心修正】新增一个集合，用于记录所有已经被拯救过的节点 ---
    set<int> all_recovered_ids;

    // 3. 逐个添加阻塞节点，生成后续步骤
    for (size_t i = 0; i < blocking_nodes.size(); ++i) {
        SimulationStep current_step;
        current_step.step = i + 1;
        vector<int> current_blocking_subset(blocking_nodes.begin(), blocking_nodes.begin() + i + 1);
        vector<double> current_probs = g.calculate_final_probabilities(initial_nodes, 10000, current_blocking_subset);
        
        set<int> current_active_ids;
        for(size_t j = 0; j < current_probs.size(); ++j) {
            if (current_probs[j] > 0.5) {
                current_step.node_states.push_back({(int)j, "active", current_probs[j]});
                current_active_ids.insert(j);
            } else if (current_probs[j] > 1e-6) {
                current_step.node_states.push_back({(int)j, "inactive", current_probs[j]});
            }
        }

        // 比较上一步和这一步的状态，找出“刚刚”获救的节点
        for (int node_id : previously_active_ids) {
            // 条件1: 节点现在不是激活状态
            // 条件2: 这个节点从未被记录到“已拯救”名单中
            if (current_active_ids.find(node_id) == current_active_ids.end() &&
                all_recovered_ids.find(node_id) == all_recovered_ids.end()) 
            {
                current_step.newly_recovered_nodes.push_back(node_id);
                all_recovered_ids.insert(node_id); // 将其加入“已拯救”名单，防止重复
            }
        }
        
        result.simulation_steps.push_back(current_step);
        previously_active_ids = current_active_ids; // 更新状态，为下一步做准备
    }
    
    result.total_steps = result.simulation_steps.size() - 1;
    return result;
}

// 【新增】将这个完整的函数粘贴到 influence_calculator.cpp 中

// 辅助函数：递归计算节点的深度（复用之前的版本）
int get_node_depth_for_critical_path(int node, const map<int, int>& parent_map, map<int, int>& memo) {
    if (memo.count(node)) return memo[node];
    if (!parent_map.count(node) || parent_map.at(node) == -1) {
        memo[node] = 0;
        return 0;
    }
    int parent = parent_map.at(node);
    int depth = 1 + get_node_depth_for_critical_path(parent, parent_map, memo);
    memo[node] = depth;
    return depth;
}

ApiCriticalPathResult find_critical_paths(
    const string& result_id,
    const string& dataset_id,
    const string& propagation_model,
    const string& probability_model,
    const vector<int>& initial_nodes
) {
    ApiCriticalPathResult result;
    result.result_id = result_id;

    // 1. 加载图并设置模型
    std::string graph_filepath = "./" + dataset_id + "_subset_1000.txt";
    InfGraph g(graph_filepath);
    g.setInfuModel(model_str_to_enum(propagation_model));
    g.setActiveProbabilityModel(probability_model);

    // 2. 运行一次模拟以获取传播树
    map<int, int> parent_map = g.run_forward_simulation_with_parent_tracking(initial_nodes, {});
    if (parent_map.empty()) {
        result.message = "模拟未产生任何激活节点，无法找到路径。";
        return result;
    }
    
    // 3. 找到最深的节点
    map<int, int> depth_memo;
    int max_depth = -1;
    int deepest_node = -1;

    for (auto const& [child, parent] : parent_map) {
        int depth = get_node_depth_for_critical_path(child, parent_map, depth_memo);
        if (depth > max_depth) {
            max_depth = depth;
            deepest_node = child;
        }
    }

    if (deepest_node == -1) {
        result.message = "未能确定最深路径。";
        return result;
    }

    // 4. 从最深的节点回溯以构建路径
    CriticalPath path;
    path.type = "deepest";
    path.score = max_depth;
    vector<int> node_sequence;
    int current_node = deepest_node;
    while (current_node != -1) {
        node_sequence.push_back(current_node);
        if (!parent_map.count(current_node)) break; // 安全检查
        current_node = parent_map.at(current_node);
    }
    std::reverse(node_sequence.begin(), node_sequence.end()); // 反转为从种子到末端的顺序
    path.nodes = node_sequence;
    
    result.critical_paths.push_back(path);
    result.message = "Successfully found a deepest propagation path with length " + std::to_string(max_depth) + ".";
    return result;
}