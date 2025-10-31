#ifndef INFGRAPH_H
#define INFGRAPH_H

#include "graph.h"
#include "iheap.h"
#include <map> // 为了使用 std::map

#include "sfmt/SFMT.h"
#include "api_structures.h" // 引入所有API数据结构

// 算法参数结构体
struct Argument
{
    int k;
    double epsilon;
    string dataset;
    string model;
};

// 传播模型枚举
enum InfluModel
{
    IC,
    LT,
    WC
};

class InfGraph : public Graph
{
private:
    sfmt_t sfmt; // 随机数生成器状态

    // --- 私有模拟辅助函数 ---

    // 为IC模型生成单个反向可达集(RR set)
    void generate_rr_set_ic(int start_node, int rr_set_idx)
    {
        vector<int> q;
        q.push_back(start_node);
        hyperGT[rr_set_idx].push_back(start_node);
        hyperG[start_node].push_back(rr_set_idx);
        vector<bool> visited(n, false);
        visited[start_node] = true;
        int head = 0;
        while (head < (int)q.size())
        {
            int u = q[head++];
            for (size_t i = 0; i < gT[u].size(); ++i)
            {
                int v = gT[u][i];
                double p = (*active_probT)[u][i];
                if (!visited[v] && sfmt_genrand_real1(&sfmt) < p)
                {
                    visited[v] = true;
                    q.push_back(v);
                    hyperGT[rr_set_idx].push_back(v);
                    hyperG[v].push_back(rr_set_idx);
                }
            }
        }
    }

    // 为LT模型生成单个反向可达集(RR set)
    void generate_rr_set_lt(int start_node, int rr_set_idx)
    {
        vector<int> q;
        q.push_back(start_node);
        hyperGT[rr_set_idx].push_back(start_node);
        hyperG[start_node].push_back(rr_set_idx);

        vector<bool> visited(n, false);
        visited[start_node] = true;
        int head = 0;

        while (head < (int)q.size())
        {
            int u = q[head++]; // 当前节点 u

            if (gT[u].empty())
                continue;

            // --- 【核心修改】使用轮盘赌选择（Weighted Random Selection）---

            // 1. 生成一个 (0, 1] 之间的随机数
            double rand_val = sfmt_genrand_real1(&sfmt);

            // 2. 遍历 u 的所有入邻居，模拟轮盘赌
            for (size_t i = 0; i < gT[u].size(); ++i)
            {
                // 从 active_probT 获取这条边的权重
                double edge_weight = (*active_probT)[u][i];

                rand_val -= edge_weight; // 减去当前边的权重

                // 3. 如果随机数小于等于0，说明随机数落在了当前边的区间内
                if (rand_val <= 0)
                {
                    int v = gT[u][i]; // 选中的邻居节点 v

                    // 4. 如果邻居节点 v 之前没有被访问过，则将其加入RR set
                    if (!visited[v])
                    {
                        visited[v] = true;
                        q.push_back(v);
                        hyperGT[rr_set_idx].push_back(v);
                        hyperG[v].push_back(rr_set_idx);
                    }

                    // 5. 找到一个节点后，必须立即终止内层循环
                    break;
                }
            }
        }
    }

public:
    InfluModel influModel;
    const vector<vector<double>> *active_probT = nullptr;
    vector<vector<int>> hyperG;
    vector<vector<int>> hyperGT;
    vector<int> result_node_set;                            // 通用名，可用于种子集或阻塞集
    const vector<vector<double>> *active_probFwd = nullptr; // 【新增】用于前向模拟的概率指针

    InfGraph(const string &graph_filepath) : Graph(graph_filepath)
    {
        sfmt_init_gen_rand(&sfmt, 1234);
    }

    // --- 模型与概率设置 ---
    void setInfuModel(InfluModel p) { influModel = p; }
    // 在 infgraph.h 的 class InfGraph 内部

    void setActiveProbabilityModel(const std::string &model_name)
    {
        if (model_name == "WC")
        {
            active_probT = &this->prob_wc;
            active_probFwd = &this->prob_fwd_wc; // 【新增】
        }
        else if (model_name == "TR")
        {
            active_probT = &this->prob_tr;
            active_probFwd = &this->prob_fwd_tr; // 【新增】
        }
        else if (model_name == "CO")
        {
            active_probT = &this->prob_co;
            active_probFwd = &this->prob_fwd_co; // 【新增】
        }
        else
        {
            throw std::invalid_argument("Unknown probability model: " + model_name);
        }
    }
    // --- 核心 RR Set/超图 操作 ---
    void init_hyper_graph()
    {
        hyperG.assign(n, vector<int>());
        hyperGT.clear();
    }

    void build_hyper_graph_r(int64_t R)
    {
        assert(active_probT != nullptr && "Probability model must be set.");
        if ((size_t)R > hyperGT.capacity())
            hyperGT.reserve(R);
        for (int i = 0; i < R; i++)
        {
            hyperGT.push_back(vector<int>());
            int random_node = sfmt_genrand_uint32(&sfmt) % n;
            if (influModel == IC || influModel == WC)
                generate_rr_set_ic(random_node, i);
            else if (influModel == LT)
                generate_rr_set_lt(random_node, i);
        }
    }

    void build_hyper_graph_from_targets(const vector<int> &target_nodes, int64_t R)
    {
        assert(!target_nodes.empty() && "Target node set cannot be empty.");
        assert(active_probT != nullptr && "Probability model must be set.");
        init_hyper_graph();
        if ((size_t)R > hyperGT.capacity())
            hyperGT.reserve(R);
        for (int i = 0; i < R; i++)
        {
            hyperGT.push_back(vector<int>());
            int start_node = target_nodes[sfmt_genrand_uint32(&sfmt) % target_nodes.size()];
            if (influModel == IC || influModel == WC)
                generate_rr_set_ic(start_node, i);
            else if (influModel == LT)
                generate_rr_set_lt(start_node, i);
        }
    }

    // 【新增】将这个完整的函数粘贴到 influence_calculator.cpp 的顶部区域
    // 【替换】run_forward_simulation_with_parent_tracking 的完整实现
    map<int, pair<int, double>> run_forward_simulation_with_tracking(const vector<int>& initial_nodes, const vector<int>& blocking_nodes) {
        map<int, pair<int, double>> parent_map; // <子节点, {父节点, 边的概率}>
        if (initial_nodes.empty()) return parent_map;

        vector<bool> activated(n, false);
        vector<bool> is_blocked(n, false);
        for(int node : blocking_nodes) is_blocked[node] = true;

        queue<int> q;
        for (int seed : initial_nodes) {
            if (!is_blocked[seed]) {
                q.push(seed);
                activated[seed] = true;
                parent_map[seed] = {-1, 1.0}; // 种子节点没有父节点，概率设为1.0
            }
        }

        if (influModel == LT) {
            // LT模型的逻辑保持不变，因为它不依赖单边概率
            vector<double> thresholds(n);
            for(int i=0; i<n; ++i) thresholds[i] = sfmt_genrand_real1(&sfmt);
            vector<double> total_weights(n, 0.0);
            
            queue<int> lt_q = q;
            while(!lt_q.empty()){
                int u = lt_q.front();
                lt_q.pop();

                for (size_t j = 0; j < g[u].size(); ++j) {
                    int v = g[u][j];
                    if (activated[v] || is_blocked[v]) continue;

                    double weight = (*active_probFwd)[u][j];
                    total_weights[v] += weight;

                    if(total_weights[v] >= thresholds[v]){
                        activated[v] = true;
                        lt_q.push(v);
                        if (!parent_map.count(v)) {
                            parent_map[v] = {u, weight}; // 记录第一个激活它的边的权重
                        }
                    }
                }
            }
        } else { // IC 和 WC 模型的模拟逻辑
            while (!q.empty()) {
                int u = q.front();
                q.pop();

                for (size_t j = 0; j < g[u].size(); ++j) {
                    int v = g[u][j];
                    if (activated[v] || is_blocked[v]) continue;

                    double prob = (*active_probFwd)[u][j];
                    if (sfmt_genrand_real1(&sfmt) < prob) {
                        activated[v] = true;
                        q.push(v);
                        parent_map[v] = {u, prob}; // 【核心修改】同时记录父节点和边的概率
                    }
                }
            }
        }
        return parent_map;
    }


    // 【替换】find_main_propagation_paths 的完整实现
    vector<Edge> find_main_propagation_paths(const vector<int> &seed_nodes)
    {
        if (seed_nodes.empty())
        {
            return {};
        }

        // 1. 运行增强版的模拟，获取包含概率的父子关系图
        map<int, pair<int, double>> parent_prob_map = run_forward_simulation_with_tracking(seed_nodes, {});
        
        // 2. 将 map 转换为一个可以排序的 vector
        vector<pair<double, Edge>> weighted_edges;
        for (auto const &[child, parent_info] : parent_prob_map)
        {
            int parent = parent_info.first;
            double prob = parent_info.second;
            if (parent != -1) // 排除种子节点
            {
                weighted_edges.push_back({prob, {parent, child}});
            }
        }

        // 3. 按概率从高到低排序
        std::sort(weighted_edges.begin(), weighted_edges.end(), 
            [](const pair<double, Edge>& a, const pair<double, Edge>& b) {
                return a.first > b.first; // 只比较概率（第一个元素），并按降序排列
            }
        );

        // 4. 提取概率最高的边，最多不超过50条
        vector<Edge> main_paths;
        for (size_t i = 0; i < weighted_edges.size() && i < 50; ++i)
        {
            main_paths.push_back(weighted_edges[i].second);
        }

        return main_paths;
    }

    void build_blocking_set(int k, const vector<int> &negative_seeds)
    {
        result_node_set.clear();

        // 1. 创建负面种子的快速查找集合
        vector<bool> is_negative_seed(n, false);
        for (int seed : negative_seeds)
        {
            if (seed >= 0 && seed < n)
                is_negative_seed[seed] = true;
        }

        // 2. 识别所有“风险RR集”（即包含了至少一个负面种子的RR set）
        vector<int> risky_rr_indices;
        vector<bool> is_rr_risky(hyperGT.size(), false);

        for (int seed : negative_seeds)
        {
            for (int rr_idx : hyperG[seed])
            {
                if (!is_rr_risky[rr_idx])
                {
                    is_rr_risky[rr_idx] = true;
                    risky_rr_indices.push_back(rr_idx);
                }
            }
        }

        // 3. 计算每个【非负面种子】节点在【风险RR集】中的出现次数（度）
        iHeap<double> degree_heap;
        degree_heap.initialize(n);
        vector<int> node_degrees(n, 0);

        for (int rr_idx : risky_rr_indices)
        {
            for (int node : hyperGT[rr_idx])
            {
                // 我们只关心那些可以作为阻塞节点的候选者
                if (!is_negative_seed[node])
                {
                    node_degrees[node]++;
                }
            }
        }

        for (int i = 0; i < n; ++i)
        {
            if (!is_negative_seed[i] && node_degrees[i] > 0)
            {
                degree_heap.insert(i, -static_cast<double>(node_degrees[i]));
            }
        }

        // 4. 贪心选择覆盖最多“风险RR集”的阻塞节点
        vector<bool> covered(hyperGT.size(), false);
        for (int i = 0; i < k && !degree_heap.empty(); i++)
        {
            int max_node = degree_heap.pop(); // 选出当前覆盖率最高的阻塞节点
            result_node_set.push_back(max_node);

            // 更新其他候选节点的覆盖度
            for (int rr_idx : hyperG[max_node])
            {
                if (is_rr_risky[rr_idx] && !covered[rr_idx])
                {
                    covered[rr_idx] = true;
                    for (int node_in_rr : hyperGT[rr_idx])
                    {
                        if (!is_negative_seed[node_in_rr] && !degree_heap.pos.notexist(node_in_rr))
                        {
                            double current_degree = degree_heap.m_data[degree_heap.pos.get(node_in_rr)].value;
                            degree_heap.insert(node_in_rr, current_degree + 1.0);
                        }
                    }
                }
            }
        }
    }

    // 【新增】为IC模型优化的、带提前终止功能的RR set生成函数
    void generate_rr_set_ic_stoppable(
        int start_node,
        int rr_set_idx,
        const vector<bool> &is_target // 快速查找的目标集
    )
    {
        if (is_target[start_node])
        { // 如果起始点就是目标，直接完成
            hyperGT[rr_set_idx].push_back(start_node);
            hyperG[start_node].push_back(rr_set_idx);
            return;
        }

        vector<int> q;
        q.push_back(start_node);
        hyperGT[rr_set_idx].push_back(start_node);
        hyperG[start_node].push_back(rr_set_idx);

        vector<bool> visited(n, false);
        visited[start_node] = true;
        int head = 0;

        while (head < (int)q.size())
        {
            int u = q[head++];
            for (size_t i = 0; i < gT[u].size(); ++i)
            {
                int v = gT[u][i];
                double p = (*active_probT)[u][i];

                if (!visited[v] && sfmt_genrand_real1(&sfmt) < p)
                {
                    visited[v] = true;
                    q.push_back(v);
                    hyperGT[rr_set_idx].push_back(v);
                    hyperG[v].push_back(rr_set_idx);

                    // 【【核心优化】】
                    // 如果新加入的节点是目标之一，立即停止扩展此RR set
                    if (is_target[v])
                    {
                        return;
                    }
                }
            }
        }
    }

    // 【新增】为LT模型优化的、带提前终止功能的RR set生成函数
    void generate_rr_set_lt_stoppable(
        int start_node,
        int rr_set_idx,
        const vector<bool> &is_target)
    {
        if (is_target[start_node])
        {
            hyperGT[rr_set_idx].push_back(start_node);
            hyperG[start_node].push_back(rr_set_idx);
            return;
        }

        vector<int> q;
        q.push_back(start_node);
        hyperGT[rr_set_idx].push_back(start_node);
        hyperG[start_node].push_back(rr_set_idx);

        vector<bool> visited(n, false);
        visited[start_node] = true;
        int head = 0;

        while (head < (int)q.size())
        {
            int u = q[head++];
            if (gT[u].empty())
                continue;

            // ... (LT的轮盘赌选择逻辑不变) ...
            double rand_val = sfmt_genrand_real1(&sfmt);
            for (size_t i = 0; i < gT[u].size(); ++i)
            {
                double edge_weight = (*active_probT)[u][i];
                rand_val -= edge_weight;
                if (rand_val <= 0)
                {
                    int v = gT[u][i];
                    if (!visited[v])
                    {
                        visited[v] = true;
                        q.push_back(v);
                        hyperGT[rr_set_idx].push_back(v);
                        hyperG[v].push_back(rr_set_idx);

                        // 【【核心优化】】
                        if (is_target[v])
                        {
                            return;
                        }
                    }
                    break;
                }
            }
        }
    }

    // --- 集合选择与影响力估算 ---
    // 在 infgraph.h 的 class InfGraph 中

    void build_hyper_graph_for_minimization(int64_t R, const vector<int> &negative_seeds)
    {
        assert(active_probT != nullptr && "Probability model must be set.");
        if (hyperGT.capacity() < (size_t)R)
            hyperGT.reserve(R);

        // 创建一个负面种子的快速查找表
        vector<bool> is_negative_seed(n, false);
        for (int seed : negative_seeds)
        {
            if (seed >= 0 && seed < n)
                is_negative_seed[seed] = true;
        }

        for (int i = 0; i < R; i++)
        {
            hyperGT.push_back(vector<int>());
            int random_node = sfmt_genrand_uint32(&sfmt) % n;

            // 调用我们新增的、带提前终止优化的函数
            if (influModel == IC || influModel == WC)
            {
                generate_rr_set_ic_stoppable(random_node, i, is_negative_seed);
            }
            else if (influModel == LT)
            {
                generate_rr_set_lt_stoppable(random_node, i, is_negative_seed);
            }
        }
    }

    void build_max_coverage_set(int k, const vector<int> &excluded_nodes = {})
    {
        result_node_set.clear();

        // 为了快速查找，将被排除的节点放入一个set或bool数组
        vector<bool> is_excluded(n, false);
        for (int node : excluded_nodes)
        {
            if (node >= 0 && node < n)
            {
                is_excluded[node] = true;
            }
        }

        iHeap<double> degree_heap;
        degree_heap.initialize(n);
        for (int i = 0; i < n; i++)
        {
            // 【【核心修改】】
            // 如果节点被排除了，或者它没有任何覆盖，就跳过
            if (is_excluded[i] || hyperG[i].empty())
            {
                continue;
            }
            degree_heap.insert(i, -static_cast<double>(hyperG[i].size()));
        }

        vector<bool> covered(hyperGT.size(), false);
        for (int i = 0; i < k && !degree_heap.empty(); i++)
        {
            // ... 后续的贪心选择逻辑保持不变 ...
            int max_node = degree_heap.pop();
            result_node_set.push_back(max_node);
            for (int rr_set_idx : hyperG[max_node])
            {
                if (!covered[rr_set_idx])
                {
                    covered[rr_set_idx] = true;
                    for (int node_in_rr_set : hyperGT[rr_set_idx])
                    {
                        if (is_excluded[node_in_rr_set] || degree_heap.pos.notexist(node_in_rr_set))
                            continue; // 【可选优化】
                        double current_degree = degree_heap.m_data[degree_heap.pos.get(node_in_rr_set)].value;
                        degree_heap.insert(node_in_rr_set, current_degree + 1.0);
                    }
                }
            }
        }
    }

    double InfluenceHyperGraph()
    {
        if (result_node_set.empty() || hyperGT.empty())
            return 0.0;
        vector<bool> covered(hyperGT.size(), false);
        int count = 0;
        for (int node : result_node_set)
        {
            for (int rr_set_idx : hyperG[node])
            {
                if (!covered[rr_set_idx])
                {
                    covered[rr_set_idx] = true;
                    count++;
                }
            }
        }
        return static_cast<double>(count) / hyperGT.size() * n;
    }

    double estimate_influence(const vector<int> &seed_nodes, const vector<int> &blocking_nodes, int iterations = 200000)
    {
        init_hyper_graph();
        build_hyper_graph_r(iterations);
        vector<bool> covered(hyperGT.size(), false);
        vector<bool> is_blocked_rr(hyperGT.size(), false);
        for (int blocker : blocking_nodes)
        {
            for (int rr_set_idx : hyperG[blocker])
            {
                is_blocked_rr[rr_set_idx] = true;
            }
        }
        int count = 0;
        for (int seed : seed_nodes)
        {
            for (int rr_set_idx : hyperG[seed])
            {
                if (!is_blocked_rr[rr_set_idx] && !covered[rr_set_idx])
                {
                    covered[rr_set_idx] = true;
                    count++;
                }
            }
        }
        return static_cast<double>(count) / hyperGT.size() * n;
    }

    vector<double> calculate_final_probabilities(
        const vector<int> &initial_nodes,
        int num_simulations,
        const vector<int> &blocking_nodes = {} // 【新增】第三个参数
    )
    {
        assert(active_probFwd != nullptr && "Forward probability model must be set.");
        assert(num_simulations > 0 && "Number of simulations must be positive.");

        vector<double> influence_counts(n, 0.0);

        // 【新增】创建一个快速查找表来标记阻塞节点
        vector<bool> is_blocked(n, false);
        for (int node : blocking_nodes)
        {
            if (node >= 0 && node < n)
                is_blocked[node] = true;
        }

        // --- 主循环：执行 num_simulations 次独立的模拟 ---
        for (int i = 0; i < num_simulations; ++i)
        {
            vector<bool> activated(n, false);
            queue<int> q;

            // 初始化种子节点
            for (int seed : initial_nodes)
            {
                // 【修改】如果种子节点本身被阻塞，它不能启动传播
                if (seed >= 0 && seed < n && !is_blocked[seed] && !activated[seed])
                {
                    activated[seed] = true;
                    q.push(seed);
                }
            }

            // --- 根据不同的模型执行单次模拟 ---
            if (influModel == LT)
            {
                // LT 模型模拟逻辑
                vector<double> thresholds(n);
                for (int j = 0; j < n; ++j)
                {
                    thresholds[j] = sfmt_genrand_real1(&sfmt);
                }
                vector<double> total_weights(n, 0.0);

                queue<int> lt_q = q; // 为LT创建一个单独的队列副本
                while (!lt_q.empty())
                {
                    int u = lt_q.front();
                    lt_q.pop();

                    for (size_t j = 0; j < g[u].size(); ++j)
                    {
                        int v = g[u][j];
                        // 【修改】如果邻居已被激活或被阻塞，则跳过
                        if (activated[v] || is_blocked[v])
                            continue;

                        double weight = (*active_probFwd)[u][j];
                        total_weights[v] += weight;

                        if (total_weights[v] >= thresholds[v])
                        {
                            activated[v] = true;
                            lt_q.push(v);
                        }
                    }
                }
            }
            else
            { // IC 模型的模拟逻辑
                while (!q.empty())
                {
                    int u = q.front();
                    q.pop();

                    for (size_t j = 0; j < g[u].size(); ++j)
                    {
                        int v = g[u][j];
                        // 【修改】如果邻居已被激活或被阻塞，则跳过
                        if (activated[v] || is_blocked[v])
                            continue;

                        double prob = (*active_probFwd)[u][j];
                        if (sfmt_genrand_real1(&sfmt) < prob)
                        {
                            activated[v] = true;
                            q.push(v);
                        }
                    }
                }
            }

            // 统计本次模拟中所有被激活的节点
            for (int j = 0; j < n; ++j)
            {
                if (activated[j])
                {
                    influence_counts[j]++;
                }
            }
        }

        // --- 计算最终的概率期望 ---
        for (int j = 0; j < n; ++j)
        {
            influence_counts[j] /= num_simulations;
        }

        return influence_counts;
    }

    // in infgraph.h, inside class InfGraph

    ApiSimulationResult run_probability_simulation(
        const vector<int> &initial_nodes,
        const vector<int> &blocking_nodes = {},
        int max_steps = 10,
        double threshold = 0.5,
        double stop_delta = 1e-6)
    {
        assert(active_probT != nullptr && "Probability model must be set.");
    
        ApiSimulationResult result;
        result.total_steps = 0;
    
        vector<bool> is_blocked(n, false);
        for (int node : blocking_nodes)
        {
            if (node >= 0 && node < n)
                is_blocked[node] = true;
        }
    
        vector<double> current_prob(n, 0.0);
        vector<double> next_prob(n, 0.0);
    
        // --- Step 0: 初始化 ---
        SimulationStep step0;
        step0.step = 0;
        for (int seed : initial_nodes)
        {
            if (seed >= 0 && seed < n && !is_blocked[seed])
            {
                next_prob[seed] = 1.0;
                step0.node_states.push_back({seed, "active", 1.0});
            }
        }
        result.simulation_steps.push_back(step0);
    
        // --- 迭代传播 ---
        for (int i = 1; i < max_steps; ++i)
        {
            current_prob = next_prob;
            bool changed = false;
    
            // IC 和 LT 模型的迭代逻辑 (这部分不变)
            if (influModel == IC)
            {
                // ... IC 逻辑 (无变化)
                for (int v = 0; v < n; ++v)
                {
                    if (is_blocked[v] || current_prob[v] > 1.0 - stop_delta) {
                        next_prob[v] = current_prob[v];
                        continue;
                    }
                    double p_not_activated = 1.0;
                    for (size_t j = 0; j < gT[v].size(); ++j) {
                        int u = gT[v][j];
                        double edge_prob = (*active_probT)[v][j];
                        p_not_activated *= (1.0 - current_prob[u] * edge_prob);
                    }
                    next_prob[v] = 1.0 - p_not_activated;
                    if (abs(next_prob[v] - current_prob[v]) > stop_delta) {
                        changed = true;
                    }
                }
            }
            else if (influModel == LT)
            {
                // ... LT 逻辑 (无变化)
                for (int v = 0; v < n; ++v)
                {
                    if (is_blocked[v] || current_prob[v] > 1.0 - stop_delta) {
                        next_prob[v] = current_prob[v];
                        continue;
                    }
                    double sum_prob = 0.0;
                    for (size_t j = 0; j < gT[v].size(); ++j) {
                        int u = gT[v][j];
                        double edge_weight = (*active_probT)[v][j];
                        sum_prob += current_prob[u] * edge_weight;
                    }
                    next_prob[v] = std::min(1.0, sum_prob);
                    if (abs(next_prob[v] - current_prob[v]) > stop_delta) {
                        changed = true;
                    }
                }
            }
    
            // 如果概率值完全收敛，也提前停止
            if (!changed)
                break;
    
            // --- 记录当前步骤的快照 ---
            SimulationStep current_step;
            current_step.step = i;
    
            for (int node_id = 0; node_id < n; ++node_id)
            {
                double new_p = next_prob[node_id];
                double old_p = current_prob[node_id];
    
                // 记录有变化的节点
                if (abs(new_p - old_p) > stop_delta || (old_p < threshold && new_p >= threshold))
                {
                    string state = (new_p >= threshold) ? "active" : "inactive";
                    current_step.node_states.push_back({node_id, state, new_p});
                }
                
                // 记录新激活的节点
                if (old_p < threshold && new_p >= threshold)
                {
                    current_step.newly_activated_nodes.push_back(node_id);
                }
            }
            
            // 如果当前步骤有变化，则记录下来
            if (!current_step.node_states.empty())
            {
                result.simulation_steps.push_back(current_step);
                result.total_steps = i;
            }
    
            if (current_step.newly_activated_nodes.empty())
            {
                break;
            }
        }
        return result;
    }

    vector<int> generate_random_seeds(int k)
    {
        vector<int> seeds;
        if (k <= 0)
            return seeds;
        if (k >= n)
        { // 如果 k 大于等于节点总数，则返回所有节点
            seeds.resize(n);
            for (int i = 0; i < n; ++i)
                seeds[i] = i;
            return seeds;
        }

        seeds.resize(k);
        vector<int> candidates(n);
        // 使用 iota 快速填充 0, 1, 2, ..., n-1
        std::iota(candidates.begin(), candidates.end(), 0);

        // 使用 std::random_device 提供更高质量的随机种子
        std::random_device rd;
        std::mt19937 g(rd());

        // 使用 std::shuffle 进行高效、现代的随机打乱
        std::shuffle(candidates.begin(), candidates.end(), g);

        // 取前 k 个作为种子
        for (int i = 0; i < k; i++)
        {
            seeds[i] = candidates[i];
        }

        return seeds;
    }

    vector<int> generate_high_degree_seeds(int k)
    {
        vector<int> seeds;
        if (k <= 0)
            return seeds;
        // 如果k大于等于节点总数，返回所有节点
        if (k >= n)
        {
            seeds.resize(n);
            std::iota(seeds.begin(), seeds.end(), 0);
            return seeds;
        }

        vector<pair<int, int>> node_out_degrees;
        node_out_degrees.reserve(n);
        for (int i = 0; i < n; ++i)
        {
            // 使用出度 (g[i].size()) 作为衡量传播能力的指标
            node_out_degrees.push_back({(int)g[i].size(), i});
        }

        // 按度数从高到低排序
        std::sort(node_out_degrees.rbegin(), node_out_degrees.rend());

        seeds.reserve(k);
        for (int i = 0; i < k; ++i)
        {
            seeds.push_back(node_out_degrees[i].second); // .second 存储的是节点ID
        }

        return seeds;
    }

    vector<Edge> find_cut_off_edges( // 函数名也修改以反映新功能
        const vector<int> &negative_seeds,
        const vector<int> &blocking_nodes)
    {
        vector<Edge> cut_off_edges;
        if (negative_seeds.empty())
            return cut_off_edges;

        // --- 1. 模拟“阻塞前”的传播，并记录父子关系 ---
        map<int, int> original_parent_map = this->run_forward_simulation_with_parent_tracking(negative_seeds, {});
        if (original_parent_map.empty())
            return cut_off_edges;

        // --- 2. 模拟“阻塞后”的传播，记录激活状态 ---
        vector<double> after_probs = this->calculate_final_probabilities(negative_seeds, 1, blocking_nodes);
        vector<bool> is_activated_after_blocking(n, false);
        for (size_t j = 0; j < after_probs.size(); ++j)
        {
            if (after_probs[j] > 0.5)
            {
                is_activated_after_blocking[j] = true;
            }
        }

        // --- 3. 对比，找出被切断的边 ---
        // 遍历原始传播中的所有“父 -> 子”关系
        for (auto const &[child, parent] : original_parent_map)
        {
            // 种子节点的父是-1，不是一条真实的边，跳过
            if (parent == -1)
                continue;

            // 【核心逻辑】
            // 如果一个孩子节点在原始传播中被激活，但在阻塞后未能被激活，
            // 那么，激活它的那条边 parent -> child 就是一条被切断的关键边。
            if (!is_activated_after_blocking[child])
            {
                cut_off_edges.push_back({parent, child});
            }
        }

        return cut_off_edges;
    }

    map<int, int> run_forward_simulation_with_parent_tracking(
        const vector<int> &initial_nodes,
        const vector<int> &blocking_nodes = {})
    {
        map<int, int> parent_map;
        if (initial_nodes.empty())
            return parent_map;

        assert(active_probFwd != nullptr && "Forward probability model must be set for simulation.");

        vector<bool> is_blocked(n, false);
        for (int node : blocking_nodes)
        {
            if (node >= 0 && node < n)
                is_blocked[node] = true;
        }

        vector<bool> activated(n, false);
        queue<int> q;

        // 初始化种子节点
        for (int seed : initial_nodes)
        {
            if (seed >= 0 && seed < n && !is_blocked[seed] && !activated[seed])
            {
                activated[seed] = true;
                q.push(seed);
                parent_map[seed] = -1; // 标记为根节点
            }
        }

        // --- 根据不同的模型执行单次模拟，并记录父节点 ---
        if (influModel == LT)
        {
            // LT 模型模拟逻辑
            vector<double> thresholds(n);
            for (int j = 0; j < n; ++j)
            {
                thresholds[j] = sfmt_genrand_real1(&sfmt);
            }
            vector<double> total_weights(n, 0.0);

            queue<int> lt_q = q;
            while (!lt_q.empty())
            {
                int u = lt_q.front();
                lt_q.pop();

                for (size_t j = 0; j < g[u].size(); ++j)
                {
                    int v = g[u][j];
                    if (activated[v] || is_blocked[v])
                        continue;

                    double weight = (*active_probFwd)[u][j];
                    total_weights[v] += weight;

                    if (total_weights[v] >= thresholds[v])
                    {
                        activated[v] = true;
                        lt_q.push(v);
                        // 记录第一个使节点跨过阈值的邻居为父节点
                        if (!parent_map.count(v))
                        {
                            parent_map[v] = u;
                        }
                    }
                }
            }
        }
        else
        { // IC 和 WC 模型的模拟逻辑
            while (!q.empty())
            {
                int u = q.front();
                q.pop();

                for (size_t j = 0; j < g[u].size(); ++j)
                {
                    int v = g[u][j];
                    if (activated[v] || is_blocked[v])
                        continue;

                    double prob = (*active_probFwd)[u][j];
                    if (sfmt_genrand_real1(&sfmt) < prob)
                    {
                        activated[v] = true;
                        q.push(v);
                        parent_map[v] = u; // IC 模型中父子关系明确
                    }
                }
            }
        }

        return parent_map;
    }
};
#endif
