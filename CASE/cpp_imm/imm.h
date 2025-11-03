#ifndef IMM_H
#define IMM_H

#include "infgraph.h" 
#include <cmath>     
#include <cassert>

// 数学计算辅助类
class Math {
public:
    static double log2(int n) {
        return log(n) / log(2);
    }
    static double logcnk(int n, int k) {
        if (k < 0 || k > n) return -1;
        if (k == 0 || k == n) return 0;
        if (k > n / 2) k = n - k;
        double res = 0;
        for (int i = 1; i <= k; i++) {
            res += log(n - i + 1) - log(i);
        }
        return res;
    }
};

// IMM 算法核心实现
class Imm {
private:
    static double step1(InfGraph& g, const Argument& arg) {
        double epsilon_prime = arg.epsilon * sqrt(2);
        
        for (int x = 1; ; x++) {
            int64_t ci = (2.0 + 2.0 / 3.0 * epsilon_prime) * (log(g.n) + Math::logcnk(g.n, arg.k) + log(Math::log2(g.n))) * pow(2.0, x) / (epsilon_prime * epsilon_prime);

            g.build_hyper_graph_r(ci);
            // 【修正】使用新的通用函数名
            g.build_max_coverage_set(arg.k);
            double ept = g.InfluenceHyperGraph() / g.n;
            
            if (ept > 1.0 / pow(2.0, x)) {
                double OPT_prime = ept * g.n / (1.0 + epsilon_prime);
                return OPT_prime;
            }
        }
        assert(false && "Should not reach here in step1");
        return -1;
    }

    static void step2(InfGraph& g, const Argument& arg, double OPT_prime) {
        assert(OPT_prime > 0);
        
        double e = exp(1.0);
        double alpha = sqrt(log(g.n) + log(2.0));
        double beta = sqrt((1.0 - 1.0 / e) * (Math::logcnk(g.n, arg.k) + log(g.n) + log(2.0)));
        int64_t R = (2.0 * g.n / (arg.epsilon * arg.epsilon)) * pow((1.0 - 1.0 / e) * alpha + beta, 2) / OPT_prime;

        g.build_hyper_graph_r(R);
        // 【修正】使用新的通用函数名
        g.build_max_coverage_set(arg.k);
    }

public:
    static void InfluenceMaximize(InfGraph& g, const Argument& arg) {
        g.init_hyper_graph();
        double OPT_prime = step1(g, arg);
        g.init_hyper_graph();
        step2(g, arg, OPT_prime);
    }
};

#endif // IMM_H

