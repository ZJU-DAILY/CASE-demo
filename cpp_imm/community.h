#ifndef COMMUNITY_SEARCH_H
#define COMMUNITY_SEARCH_H

#include "infgraph.h"
#include "api_structures.h"
#include <numeric>
#include <algorithm>
#include <iostream>
#include <vector>
#include <map>
#include <set> // 【新增】为 k-truss 的边集合
#include <unordered_map>
#include <unordered_set>
#include <queue>
#include <cmath>

using std::vector;
using std::unordered_map;
using std::queue;
using std::unordered_set;
using std::map; // 【新增】
using std::set; // 【新增】
using std::pair; // 【新增】

class CommunitySearcher {
private:
    /**
     * @brief 辅助函数：从有向图 g 和 gT 构建一个局部的、无向的邻接表。
     * @param g InfGraph 对象
     * @param node_set 搜索空间内的节点
     * @return map<int, set<int>> 无向邻接表
     */
    static map<int, set<int>> build_undirected_adj(InfGraph& g, const unordered_set<int>& node_set) {
        map<int, set<int>> adj;
        for (int u : node_set) {
            adj[u]; // 确保节点存在
            // 1. 遍历出邻居
            for (int v : g.g[u]) {
                if (node_set.count(v)) {
                    adj[u].insert(v);
                    adj[v].insert(u);
                }
            }
            // 2. 遍历入邻居
            for (int v : g.gT[u]) {
                if (node_set.count(v)) {
                    adj[u].insert(v);
                    adj[v].insert(u);
                }
            }
        }
        return adj;
    }

    /**
     * @brief 辅助函数：标准化边（u, v）为 (min, max)
     */
    static pair<int, int> make_edge(int u, int v) {
        return {std::min(u, v), std::max(u, v)};
    }

    /**
     * @brief 辅助函数：执行BFS来提取连通分量 (用于无向图)
     */
    static unordered_set<int> extract_connected_component(
        int start_node,
        const map<int, set<int>>& adj,
        const unordered_set<int>& candidates
    ) {
        unordered_set<int> component;
        queue<int> q;

        if (candidates.count(start_node)) {
            q.push(start_node);
            component.insert(start_node);
        }

        while (!q.empty()) {
            int u = q.front();
            q.pop();

            if (adj.count(u)) {
                for (int v : adj.at(u)) {
                    if (candidates.count(v) && !component.count(v)) {
                        component.insert(v);
                        q.push(v);
                    }
                }
            }
        }
        return component;
    }

    /**
     * @brief 辅助函数：准备初始搜索空间和查询节点
     * (从 (k,l)-core 版本中提取的通用逻辑)
     */
    static bool prepare_search_space(
        const vector<NodeState>& final_states,
        InfGraph& g,
        const vector<int>& query_nodes,
        unordered_set<int>& search_space,
        vector<int>& valid_query_nodes,
        unordered_map<int, double>& node_probs
    ) {
        // 1. 准备数据
        unordered_set<int> influenced_nodes;
        for (const auto& ns : final_states) {
            node_probs[ns.id] = ns.probability;
            influenced_nodes.insert(ns.id);
        }

        for (int qn : query_nodes) {
            if (influenced_nodes.count(qn)) {
                valid_query_nodes.push_back(qn);
            }
        }
        std::cout << "[DEBUG] Step 1: Found " << valid_query_nodes.size() << " valid (influenced) query nodes." << std::endl;
        if (valid_query_nodes.empty()) {
            std::cout << "[DEBUG] FAILURE: No query nodes found in the set of influenced nodes. Aborting." << std::endl;
            return false;
        }

        // 2. 找到包含查询节点的连通子图作为搜索空间 (弱连通)
        queue<int> q_bfs;
        q_bfs.push(valid_query_nodes[0]);
        search_space.insert(valid_query_nodes[0]);
        
        while (!q_bfs.empty()) {
            int u = q_bfs.front();
            q_bfs.pop();
            // 检查出边
            for (int v : g.g[u]) {
                if (influenced_nodes.count(v) && !search_space.count(v)) {
                    search_space.insert(v);
                    q_bfs.push(v);
                }
            }
            // 检查入边
            for (int v : g.gT[u]) {
                if (influenced_nodes.count(v) && !search_space.count(v)) {
                    search_space.insert(v);
                    q_bfs.push(v);
                }
            }
        }
        std::cout << "[DEBUG] Step 2: Identified search space (weakly connected component) with " << search_space.size() << " nodes." << std::endl;
        return true;
    }

    /**
     * @brief 辅助函数：封装最终结果
     */
    static CommunityResult package_result(
        const unordered_set<int>& final_component,
        const unordered_map<int, double>& node_probs
    ) {
        CommunityResult result;
        double prob_sum = 0.0;
        for (int node : final_component) {
            result.node_ids.push_back(node);
            if (node_probs.count(node)) {
                 prob_sum += node_probs.at(node);
            }
        }

        result.node_count = result.node_ids.size();
        if (result.node_count > 0) {
            result.average_influence_prob = prob_sum / result.node_count;
        } else {
            result.average_influence_prob = 0.0;
        }
        std::cout << "[DEBUG] --- Search Complete. Final community size: " << result.node_count << " ---" << std::endl;
        return result;
    }


public:
    /**
     * @brief [DIRECTED VERSION] 查找 (k, l)-core 社区。
     * (这是您现有的函数，保持不变)
     */
    static CommunityResult find_most_influenced_community_local(
        int k_core,
        int l_core, 
        const vector<NodeState>& final_states,
        InfGraph& g,
        const vector<int>& query_nodes
    ) {
        std::cout << "\n[DEBUG] --- Starting Community Search (Directed (k,l)-core Mode) ---" << std::endl;
        std::cout << "[DEBUG] k(in-degree)=" << k_core << ", l(out-degree)=" << l_core << ", Influenced Nodes=" << final_states.size() << ", Query Nodes=" << query_nodes.size() << std::endl;

        if (final_states.empty() || k_core < 0 || l_core < 0) {
            std::cout << "[DEBUG] FAILURE: Input state is empty or k/l is negative. Aborting." << std::endl;
            return {{}, 0.0, 0};
        }

        // 1. & 2. 准备搜索空间和查询节点
        unordered_set<int> search_space;
        vector<int> valid_query_nodes;
        unordered_map<int, double> node_probs;
        if (!prepare_search_space(final_states, g, query_nodes, search_space, valid_query_nodes, node_probs)) {
             return {{}, 0.0, 0};
        }
        
        // 3. 【(k, l)-core 特定部分】执行 (k, l)-core 分解
        std::cout << "[DEBUG] Step 3: Performing (k, l)-core decomposition (peeling)..." << std::endl;
        unordered_set<int> k_l_core_candidates = search_space;
        queue<int> removal_q;
        unordered_map<int, int> internal_in_degrees;
        unordered_map<int, int> internal_out_degrees;

        // 计算初始的内部入度和出度
        for (int u : k_l_core_candidates) {
            int in_degree = 0;
            for (int v : g.gT[u]) { // gT是转置图，gT[u]是u的入邻居
                 if(k_l_core_candidates.count(v)) in_degree++;
            }
            internal_in_degrees[u] = in_degree;

            int out_degree = 0;
            for (int v : g.g[u]) { // g是原图，g[u]是u的出邻居
                 if(k_l_core_candidates.count(v)) out_degree++;
            }
            internal_out_degrees[u] = out_degree;

            if (in_degree < k_core || out_degree < l_core) {
                removal_q.push(u);
            }
        }
        
        unordered_set<int> already_in_removal_q;
        queue<int> temp_q = removal_q;
        while(!temp_q.empty()){
            already_in_removal_q.insert(temp_q.front());
            temp_q.pop();
        }

        // 开始迭代剥离
        while (!removal_q.empty()) {
            int u = removal_q.front();
            removal_q.pop();
            
            if (!k_l_core_candidates.count(u)) continue;
            k_l_core_candidates.erase(u);

            // 1. 更新 u 的【入邻居】们的【出度】
            for (int v : g.gT[u]) {
                if (k_l_core_candidates.count(v)) {
                    internal_out_degrees[v]--;
                    if (internal_out_degrees[v] < l_core && !already_in_removal_q.count(v)) {
                       removal_q.push(v);
                       already_in_removal_q.insert(v);
                    }
                }
            }
            // 2. 更新 u 的【出邻居】们的【入度】
            for (int v : g.g[u]) {
                if (k_l_core_candidates.count(v)) {
                    internal_in_degrees[v]--;
                    if (internal_in_degrees[v] < k_core && !already_in_removal_q.count(v)) {
                       removal_q.push(v);
                       already_in_removal_q.insert(v);
                    }
                }
            }
        }
        std::cout << "[DEBUG] Step 3: ...Decomposition complete. " << k_l_core_candidates.size() << " nodes remain." << std::endl;
        if (k_l_core_candidates.empty()) {
             std::cout << "[DEBUG] FAILURE: The (k, l)-core decomposition removed all nodes in the search space." << std::endl;
             return {{}, 0.0, 0};
        }
        
        // 4. 找到一个在剥离后幸存的查询节点
        int surviving_query_node = -1;
        for (int qn : valid_query_nodes) {
            if (k_l_core_candidates.count(qn)) {
                surviving_query_node = qn;
                break;
            }
        }

        if (surviving_query_node == -1) {
            std::cout << "[DEBUG] FAILURE: No query node survived the (k, l)-core peeling process." << std::endl;
            return {{}, 0.0, 0};
        }
        std::cout << "[DEBUG] Step 4: Query node " << surviving_query_node << " survived the peeling." << std::endl;

        // 5. 从幸存节点开始，提取最终的连通 (k, l)-core 社区 (这里使用有向图的边)
        std::cout << "[DEBUG] Step 5: Extracting final connected component from the (k, l)-core candidates..." << std::endl;
        unordered_set<int> final_k_l_core_component;
        queue<int> q_conn;
        q_conn.push(surviving_query_node);
        final_k_l_core_component.insert(surviving_query_node);

        while (!q_conn.empty()) {
            int u = q_conn.front();
            q_conn.pop();
            // 再次进行双向搜索，确保弱连通性
            for (int v : g.g[u]) {
                if (k_l_core_candidates.count(v) && !final_k_l_core_component.count(v)) {
                    final_k_l_core_component.insert(v);
                    q_conn.push(v);
                }
            }
            for (int v : g.gT[u]) {
                if (k_l_core_candidates.count(v) && !final_k_l_core_component.count(v)) {
                    final_k_l_core_component.insert(v);
                    q_conn.push(v);
                }
            }
        }
        std::cout << "[DEBUG] Step 5: ...Extraction complete. Final component has " << final_k_l_core_component.size() << " nodes." << std::endl;
        
        // 6. 封装并返回最终结果
        return package_result(final_k_l_core_component, node_probs);
    }

    // ==========================================================
    // 【【【新增】】】 k-core 查找函数
    // ==========================================================
    /**
     * @brief [UNDIRECTED VERSION] 在有影响力的节点中，查找一个包含查询节点的、连通的 k-core 社区。
     *
     * 1.  **识别搜索空间**: (同上)
     * 2.  **构建无向视图**: 将搜索空间内的 g.g 和 g.gT 视为无向边，构建邻接表。
     * 3.  **k-core 分解**: 迭代地移除所有无向度数 < k 的节点。
     * 4.  **提取最终社区**: (同上)
     *
     * @param k_core 社区节点的最小内部【无向度数】约束 (k)。
     * @param final_states 所有受影响节点及其影响概率的列表。
     * @param g 图结构 (g.g 和 g.gT 将被共同用于构建无向视图)。
     * @param query_nodes 用于定位社区的种子节点。
     * @return CommunityResult 找到的最佳 k-core 社区。
     */
    static CommunityResult find_k_core_community(
        int k_core,
        const vector<NodeState>& final_states,
        InfGraph& g,
        const vector<int>& query_nodes
    ) {
        std::cout << "\n[DEBUG] --- Starting Community Search (Undirected k-core Mode) ---" << std::endl;
        std::cout << "[DEBUG] k(undirected-degree)=" << k_core << ", Influenced Nodes=" << final_states.size() << ", Query Nodes=" << query_nodes.size() << std::endl;

        if (final_states.empty() || k_core < 0) {
            std::cout << "[DEBUG] FAILURE: Input state is empty or k is negative. Aborting." << std::endl;
            return {{}, 0.0, 0};
        }

        // 1. & 2. 准备搜索空间和查询节点
        unordered_set<int> search_space;
        vector<int> valid_query_nodes;
        unordered_map<int, double> node_probs;
        if (!prepare_search_space(final_states, g, query_nodes, search_space, valid_query_nodes, node_probs)) {
             return {{}, 0.0, 0};
        }

        // 3. 【k-core 特定部分】构建无向视图并执行 k-core 分解
        std::cout << "[DEBUG] Step 3: Building undirected view and performing k-core decomposition..." << std::endl;
        map<int, set<int>> undirected_adj = build_undirected_adj(g, search_space);
        unordered_set<int> k_core_candidates = search_space;
        
        queue<int> removal_q;
        unordered_map<int, int> internal_degrees;

        // 计算初始内部无向度数
        for (int u : k_core_candidates) {
            internal_degrees[u] = undirected_adj[u].size();
            if (internal_degrees[u] < k_core) {
                removal_q.push(u);
            }
        }

        unordered_set<int> already_in_removal_q;
        queue<int> temp_q = removal_q;
        while(!temp_q.empty()){
            already_in_removal_q.insert(temp_q.front());
            temp_q.pop();
        }

        // 开始迭代剥离
        while (!removal_q.empty()) {
            int u = removal_q.front();
            removal_q.pop();

            if (!k_core_candidates.count(u)) continue;
            k_core_candidates.erase(u);

            // 当 u 被移除时，更新其邻居的内部度数
            for (int v : undirected_adj[u]) {
                if (k_core_candidates.count(v)) {
                    internal_degrees[v]--;
                    if (internal_degrees[v] < k_core && !already_in_removal_q.count(v)) {
                        removal_q.push(v);
                        already_in_removal_q.insert(v);
                    }
                }
            }
        }
        std::cout << "[DEBUG] Step 3: ...Decomposition complete. " << k_core_candidates.size() << " nodes remain." << std::endl;
        if (k_core_candidates.empty()) {
             std::cout << "[DEBUG] FAILURE: The k-core decomposition removed all nodes." << std::endl;
             return {{}, 0.0, 0};
        }

        // 4. 找到一个在剥离后幸存的查询节点
        int surviving_query_node = -1;
        for (int qn : valid_query_nodes) {
            if (k_core_candidates.count(qn)) {
                surviving_query_node = qn;
                break;
            }
        }

        if (surviving_query_node == -1) {
            std::cout << "[DEBUG] FAILURE: No query node survived the k-core peeling process." << std::endl;
            return {{}, 0.0, 0};
        }
        std::cout << "[DEBUG] Step 4: Query node " << surviving_query_node << " survived the peeling." << std::endl;

        // 5. 从幸存节点开始，提取最终的连通 k-core 社区 (使用无向邻接表)
        std::cout << "[DEBUG] Step 5: Extracting final connected component from the k-core candidates..." << std::endl;
        unordered_set<int> final_k_core_component = extract_connected_component(
            surviving_query_node, 
            undirected_adj, 
            k_core_candidates
        );
        std::cout << "[DEBUG] Step 5: ...Extraction complete. Final component has " << final_k_core_component.size() << " nodes." << std::endl;

        // 6. 封装并返回最终结果
        return package_result(final_k_core_component, node_probs);
    }

    // ==========================================================
    // 【【【新增】】】 k-truss 查找函数
    // ==========================================================
   /**
    * @brief [UNDIRECTED VERSION] 在有影响力的节点中，查找一个包含查询节点的、连通的 k-truss 社区。
    *
    * k-truss 是一种更紧密的社区结构，其中每条边都至少是 (k-2) 个三角形的一部分。
    *
    * 1.  **识别搜索空间**: (同上)
    * 2.  **构建无向视图**: (同上)
    * 3.  **计算三角支持度**: 计算视图中每条边的“支持度”（即它所属的三角形数量）。
    * 4.  **k-truss 分解**: 迭代地移除所有支持度 < (k-2) 的*边*。当一条边被移除时，它所支持的其他边的支持度也会降低，可能引发连锁移除。
    * 5.  **提取最终社区**: (同上)
    *
    * @param k_truss trussness约束 (k)。k=2 是一般图，k=3 要求每条边至少在一个三角形中。
    * @param final_states 所有受影响节点及其影响概率的列表。
    * @param g 图结构 (g.g 和 g.gT 将被共同用于构建无向视图)。
    * @param query_nodes 用于定位社区的种子节点。
    * @return CommunityResult 找到的最佳 k-truss 社区。
    */
    static CommunityResult find_k_truss_community(
        int k_truss,
        const vector<NodeState>& final_states,
        InfGraph& g,
        const vector<int>& query_nodes
    ) {
        std::cout << "\n[DEBUG] --- Starting Community Search (Undirected k-truss Mode) ---" << std::endl;
        std::cout << "[DEBUG] k(trussness)=" << k_truss << ", Influenced Nodes=" << final_states.size() << ", Query Nodes=" << query_nodes.size() << std::endl;

        if (final_states.empty() || k_truss < 2) { // k-truss 至少为2
            std::cout << "[DEBUG] FAILURE: Input state is empty or k < 2. Aborting." << std::endl;
            return {{}, 0.0, 0};
        }
        const int min_support = k_truss - 2;

        // 1. & 2. 准备搜索空间和查询节点
        unordered_set<int> search_space;
        vector<int> valid_query_nodes;
        unordered_map<int, double> node_probs;
        if (!prepare_search_space(final_states, g, query_nodes, search_space, valid_query_nodes, node_probs)) {
             return {{}, 0.0, 0};
        }

        // 3. 【k-truss 特定部分】构建无向视图
        std::cout << "[DEBUG] Step 3: Building undirected view..." << std::endl;
        map<int, set<int>> undirected_adj = build_undirected_adj(g, search_space);

        // 4. 计算三角支持度
        std::cout << "[DEBUG] Step 4: Calculating triangle supports..." << std::endl;
        map<pair<int, int>, int> edge_supports;
        // (用于加速剥离) 存储形成三角形的第三个节点
        map<pair<int, int>, vector<int>> triangle_witnesses; 
        set<pair<int, int>> current_edges;

        for (const auto& entry : undirected_adj) {
            int u = entry.first;
            vector<int> neighbors(entry.second.begin(), entry.second.end());
            for (size_t i = 0; i < neighbors.size(); ++i) {
                for (size_t j = i + 1; j < neighbors.size(); ++j) {
                    int v = neighbors[i];
                    int w = neighbors[j];
                    // 检查 (v, w) 是否是边
                    if (undirected_adj[v].count(w)) {
                        // 发现三角形 (u, v, w)
                        pair<int, int> e_uv = make_edge(u, v);
                        pair<int, int> e_uw = make_edge(u, w);
                        pair<int, int> e_vw = make_edge(v, w);
                        
                        edge_supports[e_uv]++;
                        edge_supports[e_uw]++;
                        edge_supports[e_vw]++;

                        triangle_witnesses[e_uv].push_back(w);
                        triangle_witnesses[e_uw].push_back(v);
                        triangle_witnesses[e_vw].push_back(u);

                        current_edges.insert(e_uv);
                        current_edges.insert(e_uw);
                        current_edges.insert(e_vw);
                    }
                }
            }
        }
        std::cout << "[DEBUG] Step 4: ...Found " << current_edges.size() << " edges in " << edge_supports.size() << " triangles." << std::endl;

        // 5. 执行 k-truss (边) 剥离
        std::cout << "[DEBUG] Step 5: Performing k-truss decomposition (peeling edges)..." << std::endl;
        queue<pair<int, int>> removal_q;
        for (const auto& edge : current_edges) {
            if (edge_supports[edge] < min_support) {
                removal_q.push(edge);
            }
        }

        while (!removal_q.empty()) {
            pair<int, int> edge_uv = removal_q.front();
            removal_q.pop();

            if (!current_edges.count(edge_uv)) continue; // 已经被移除了
            current_edges.erase(edge_uv);

            int u = edge_uv.first;
            int v = edge_uv.second;

            // 遍历所有 (u, v, w) 三角形
            for (int w : triangle_witnesses[edge_uv]) {
                pair<int, int> edge_uw = make_edge(u, w);
                pair<int, int> edge_vw = make_edge(v, w);

                // 检查边 (u, w)
                if (current_edges.count(edge_uw)) {
                    edge_supports[edge_uw]--;
                    if (edge_supports[edge_uw] < min_support) {
                        removal_q.push(edge_uw);
                    }
                }
                // 检查边 (v, w)
                if (current_edges.count(edge_vw)) {
                    edge_supports[edge_vw]--;
                    if (edge_supports[edge_vw] < min_support) {
                        removal_q.push(edge_vw);
                    }
                }
            }
        }
        std::cout << "[DEBUG] Step 5: ...Decomposition complete. " << current_edges.size() << " edges remain." << std::endl;

        // 6. 从幸存的边中提取节点
        unordered_set<int> k_truss_candidates;
        map<int, set<int>> k_truss_adj; // 幸存的图结构
        for (const auto& edge : current_edges) {
            k_truss_candidates.insert(edge.first);
            k_truss_candidates.insert(edge.second);
            k_truss_adj[edge.first].insert(edge.second);
            k_truss_adj[edge.second].insert(edge.first);
        }
        std::cout << "[DEBUG] Step 6: " << k_truss_candidates.size() << " nodes remain in the k-truss." << std::endl;
         if (k_truss_candidates.empty()) {
             std::cout << "[DEBUG] FAILURE: The k-truss decomposition removed all nodes." << std::endl;
             return {{}, 0.0, 0};
        }
        
        // 7. 找到一个在剥离后幸存的查询节点
        int surviving_query_node = -1;
        for (int qn : valid_query_nodes) {
            if (k_truss_candidates.count(qn)) {
                surviving_query_node = qn;
                break;
            }
        }

        if (surviving_query_node == -1) {
            std::cout << "[DEBUG] FAILURE: No query node survived the k-truss peeling process." << std::endl;
            return {{}, 0.0, 0};
        }
        std::cout << "[DEBUG] Step 7: Query node " << surviving_query_node << " survived the peeling." << std::endl;

        // 8. 从幸存节点开始，提取最终的连通 k-truss 社区
        std::cout << "[DEBUG] Step 8: Extracting final connected component from the k-truss candidates..." << std::endl;
        unordered_set<int> final_k_truss_component = extract_connected_component(
            surviving_query_node, 
            k_truss_adj, 
            k_truss_candidates
        );
        std::cout << "[DEBUG] Step 8: ...Extraction complete. Final component has " << final_k_truss_component.size() << " nodes." << std::endl;
        
        // 9. 封装并返回最终结果
        return package_result(final_k_truss_component, node_probs);
    }

};

#endif // COMMUNITY_SEARCH_H