#ifndef GRAPH_H
#define GRAPH_H

#include "head.h" 

// ... (handle_error function remains the same) ...
inline void handle_error(const char* msg) {
    // ================= 【代码修改】 结束 =================
        perror(msg);
        exit(EXIT_FAILURE);
    }

class Graph
{
public:
    int n = 0, m = 0;
    
    // Forward Graph (for forward simulation)
    vector<vector<int>> g; 
    vector<vector<double>> prob_fwd_wc;
    vector<vector<double>> prob_fwd_tr;
    vector<vector<double>> prob_fwd_co;

    // Transposed Graph (for IMM/RR sets)
    vector<vector<int>> gT; 
    vector<int> inDeg;
    vector<vector<double>> prob_wc;
    vector<vector<double>> prob_tr;
    vector<vector<double>> prob_co;

    Graph(const string& graph_filepath)
    {
        loadGraphFromEdgeList(graph_filepath);
        precompute_all_probabilities();
    }

private:
    void loadGraphFromEdgeList(const string& filename)
    {
        ifstream file(filename);
        if (!file.is_open()) 
            {
                // 使用 std::cerr 输出错误信息到标准错误流
                // 这样可以清晰地告诉用户是哪个文件打开失败了
                std::cerr << "Error: Failed to open graph file at location: " << filename << std::endl;
                
                // 既然文件打不开，后续操作无法进行，需要终止程序
                // exit(EXIT_FAILURE) 是一个常见的选择
                exit(EXIT_FAILURE);
            }

        vector<pair<int, int>> edges;
        int max_node_id = -1;
        int u, v;

        while (file >> u >> v) {
            edges.push_back({u, v});
            if (u > max_node_id) max_node_id = u;
            if (v > max_node_id) max_node_id = v;
        }
        file.close();

        this->n = max_node_id + 1;
        this->m = edges.size();
        assert(this->n > 0 && this->m > 0);

        // Allocate memory for both forward and transposed graphs
        g.resize(n);
        gT.resize(n);
        inDeg.assign(n, 0);
        prob_wc.resize(n); prob_fwd_wc.resize(n);
        prob_tr.resize(n); prob_fwd_tr.resize(n);
        prob_co.resize(n); prob_fwd_co.resize(n);

        // Build both graphs
        for (const auto& edge : edges) {
            u = edge.first;
            v = edge.second;
            g[u].push_back(v);   // Forward edge u -> v
            gT[v].push_back(u);  // Transposed edge v <- u
            inDeg[v]++;
        }
    }

    void precompute_all_probabilities() {
        // --- Precompute for Transposed Graph (gT) ---
        for (int v = 0; v < n; ++v) {
            double wc_prob = (inDeg[v] > 0) ? (1.0 / inDeg[v]) : 0;
            prob_wc[v].assign(gT[v].size(), wc_prob);
            prob_co[v].assign(gT[v].size(), 0.1);
            prob_tr[v].resize(gT[v].size());
        }

        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> distrib(0, 2);
        const double tr_probs[] = {0.1, 0.01, 0.001};
        for (int v = 0; v < n; ++v) {
            for (size_t i = 0; i < gT[v].size(); ++i) {
                prob_tr[v][i] = tr_probs[distrib(gen)];
            }
        }
        
        // --- Precompute for Forward Graph (g) ---
        // This is more complex as probabilities are tied to the target node's in-degree (for WC)
        // We need to build a map to look up the correct probability
        for (int u = 0; u < n; ++u) {
            prob_fwd_co[u].assign(g[u].size(), 0.1);
            prob_fwd_tr[u].resize(g[u].size());
            prob_fwd_wc[u].resize(g[u].size());
            for (size_t i = 0; i < g[u].size(); ++i) {
                int v = g[u][i];
                prob_fwd_wc[u][i] = (inDeg[v] > 0) ? (1.0 / inDeg[v]) : 0;
                prob_fwd_tr[u][i] = tr_probs[distrib(gen)];
            }
        }
    }
};

#endif // GRAPH_H
