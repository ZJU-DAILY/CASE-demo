import sys
import os
from flask import Flask, request, jsonify
from flask_cors import CORS  # <--- 1. 导入

# --- 将脚本所在目录添加到Python模块搜索路径 ---
# 这可以确保无论从哪里运行此脚本，它都能找到同目录下的 .so 文件
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

# --- 尝试导入C++核心模块 ---
try:
    import imm_calculator
except ImportError as e:
    print("错误：无法导入C++核心模块 'imm_calculator'。")
    print(f"详细错误: {e}")
    print("请确保您已经成功运行了 'build_backend.sh' 脚本，")
    print("并且生成的模块文件（.so）位于 py_api/ 目录下。")
    sys.exit(1)

# --- 检查数据目录是否存在 ---
if not os.path.exists('./data'):
    print("警告：'./data' 目录不存在。请确保您的数据集位于该目录下。")

app = Flask(__name__)

allowed_origins = [
    "http://10.214.131.13:8021",  # 你的服务器前端地址
    "http://localhost:8021"      # 本地开发时可能使用的主机和端口
]

# 配置CORS，允许列表中所有的来源
CORS(app, resources={r"/api/*": {"origins": allowed_origins}})
# --- 【新增】一个简单的内存缓存 ---
# key: result_id, value: 一个包含计算结果和参数的字典
# 在真实的生产环境中，您应该使用Redis或类似的持久化缓存来代替
computation_cache = {}

@app.route('/api/influence/run', methods=['POST'])
def run_influence_task():
    """
    处理影响力最大化和最小化计算的核心API端点。
    """
    json_data = request.get_json()
    if not json_data:
        return jsonify({"error": "Invalid JSON"}), 400

    try:
        req = imm_calculator.ApiRequest()
        req.dataset_id = json_data.get("dataset_id")
        req.mode = json_data.get("mode")

        params_data = json_data.get("params", {})
        req.params.propagation_model = params_data.get("propagation_model")
        req.params.probability_model = params_data.get("probability_model")
        req.params.budget = params_data.get("budget")
        req.params.seed_nodes = params_data.get("seed_nodes", [])
        # 【核心修改】
        # 从请求中获取负面种子数量和新的生成模式标识符
        # 如果前端未提供模式，则默认为 "RANDOM"
        req.params.neg_num = params_data.get("neg_num", 10) # 假设默认10个
        req.params.seed_generation_mode = params_data.get("seed_generation_mode", "RANDOM")

        print(f"接收到请求: mode={req.mode}, dataset={req.dataset_id}, k={req.params.budget}")

        if req.mode == "maximization":
            print("开始计算影响力最大化...")
            result = imm_calculator.run_influence_maximization(req)
            
            cache_payload = {
                "dataset_id": req.dataset_id,
                "propagation_model": req.params.propagation_model,
                "probability_model": req.params.probability_model,
                "initial_nodes": [node.id for node in result.seed_nodes]
            }
            computation_cache[result.result_id] = cache_payload

            # --- 【核心修改】 ---
            response_data = {
                "result_id": result.result_id,
                "seed_nodes": [{"id": node.id, "priority": node.priority} for node in result.seed_nodes],
                "final_influence": {
                    "count": result.final_influence.count,
                    "ratio": result.final_influence.ratio
                },
                "message": result.message,
                # 新增的字段，将C++的Edge列表转换为字典列表
                "main_propagation_paths": [
                    {"source": edge.source, "target": edge.target} 
                    for edge in result.main_propagation_paths
                ]
            }
            # --- 【修改结束】 ---
            
            print("计算完成。")
            return jsonify(response_data)

        elif req.mode == "minimization":
            print("开始计算影响力最小化...")
            result = imm_calculator.run_influence_minimization(req)
            
            blocking_nodes_list_of_dicts = [
                {"id": node.id, "priority": node.priority} 
                for node in result.blocking_nodes
            ]
            cut_off_paths_list_of_dicts = [
                {"source": edge.source, "target": edge.target}
                for edge in result.cut_off_paths
            ]
            
            # --- 【【【核心修正】】】 ---
            # 缓存和返回时，必须使用 result.seed_nodes,
            # 因为当用户输入为空时，C++会随机生成种子节点。
            # result.seed_nodes 存储了实际用于计算的种子节点列表。

            # 3. 填充缓存 (使用修正后的种子节点)
            cache_payload_before = {
                "dataset_id": req.dataset_id,
                "propagation_model": req.params.propagation_model,
                "probability_model": req.params.probability_model,
                "initial_nodes": result.seed_nodes,  # <-- 修正
                "blocking_nodes": []
            }
            computation_cache[result.original_result_id] = cache_payload_before

            cache_payload_after = {
                "dataset_id": req.dataset_id,
                "propagation_model": req.params.propagation_model,
                "probability_model": req.params.probability_model,
                "initial_nodes": result.seed_nodes,  # <-- 修正
                "blocking_nodes": [node['id'] for node in blocking_nodes_list_of_dicts]
            }
            computation_cache[result.blocked_result_id] = cache_payload_after

            # 4. 构建最终的JSON响应 (使用修正后的种子节点)
            response_data = {
                "original_result_id": result.original_result_id,
                "blocked_result_id": result.blocked_result_id,
                "seed_nodes": result.seed_nodes, # <-- 修正: 将实际种子节点返回给前端
                "blocking_nodes": blocking_nodes_list_of_dicts,
                "influence_before": {
                    "count": result.influence_before.count, 
                    "ratio": result.influence_before.ratio 
                },
                "influence_after": {
                    "count": result.influence_after.count, 
                    "ratio": result.influence_after.ratio 
                },
                "reduction_ratio": result.reduction_ratio,
                "cut_off_paths": cut_off_paths_list_of_dicts,
                "message": result.message
            }
            
            print("计算完成。")
            return jsonify(response_data)
        else:
            return jsonify({"error": f"Invalid mode: {req.mode}"}), 400
    except Exception as e:
        import traceback
        traceback.print_exc()
        print(f"发生错误: {e}")
        return jsonify({"error": str(e)}), 500




@app.route('/api/influence/final-state/<result_id>', methods=['GET'])
def get_final_influence_state(result_id):
    if result_id not in computation_cache:
        return jsonify({"error": "Result ID not found or has expired."}), 404

    try:
        cached_data = computation_cache[result_id]
        print(f"为 result_id: {result_id} 计算最终影响期望...")
        
        # 【【【核心修改】】】
        # 从缓存中获取阻塞节点，如果不存在则默认为空列表
        blocking_nodes = cached_data.get("blocking_nodes", [])

        # 调用我们修改后的、带有 blocking_nodes 参数的 C++ 函数
        result = imm_calculator.get_final_influence(
            dataset_id=cached_data["dataset_id"],
            propagation_model=cached_data["propagation_model"],
            probability_model=cached_data["probability_model"],
            initial_nodes=cached_data["initial_nodes"],
            blocking_nodes=blocking_nodes # 【传入】
        )

        # 【核心修改】根据新的 FinalInfluenceResult 结构体来构建JSON响应
        # 不再有 "total_steps" 和 "simulation_steps"
        response_data = {
            "result_id": result_id, # 使用请求传入的ID，保持一致性
            "total_influence": result.total_influence,
            "final_states": [
                {
                    "id": ns.id, 
                    "state": ns.state, 
                    "probability": ns.probability
                }
                # 直接遍历新的 final_states 列表
                for ns in result.final_states 
            ]
        }
        
        print("最终影响期望计算完毕。")
        return jsonify(response_data)
        
    except Exception as e:
        # 打印更详细的错误，便于调试
        import traceback
        print(f"计算最终影响时发生错误: {e}")
        traceback.print_exc()
        return jsonify({"error": str(e)}), 500

@app.route('/api/influence/step/<result_id>', methods=['GET'])
def get_probability_animation_steps(result_id):
    """
    根据结果ID，获取分步的“概率波”演变数据，并截断影响力下降的后续步骤。
    """
    if result_id not in computation_cache:
        return jsonify({"error": "Result ID not found or has expired."}), 404

    try:
        cached_data = computation_cache[result_id]
        print(f"为 result_id: {result_id} 生成概率演变动画数据...")

        dataset_id = cached_data["dataset_id"]
        prop_model = cached_data["propagation_model"]
        prob_model = cached_data["probability_model"]
        initial_nodes = cached_data["initial_nodes"]
        blocking_nodes = cached_data.get("blocking_nodes", [])

        # 1. 调用C++获取完整的、原始的动画数据
        result = imm_calculator.get_probability_animation(
            dataset_id,
            prop_model,
            prob_model,
            initial_nodes,
            blocking_nodes
        )

        # ====================================================================
        # 【核心修改】截断逻辑：过滤掉影响力下降的步骤
        # ====================================================================
        original_step_count = len(result.simulation_steps)
        print(f"从C++后端接收到的原始动画步数: {original_step_count}")
        
        filtered_steps = []
        # 初始化一个变量来追踪已知最大的影响节点数，-1保证第一步总能通过
        max_influenced_count = -1

        for step in result.simulation_steps:
            # 计算当前步骤中“active”状态的节点总数
            # 这是一个高效的生成器表达式求和
            current_influenced_count = sum(1 for ns in step.node_states if ns.state == 'active')

            if current_influenced_count >= max_influenced_count:
                # 如果当前影响节点数大于或等于已知的最大值，说明是有效步骤
                filtered_steps.append(step)
                # 更新最大值
                max_influenced_count = current_influenced_count
            else:
                # 如果当前影响节点数减少了，立即停止处理，并丢弃此步骤及之后的所有步骤
                print(f"在第 {step.step} 步检测到影响节点数下降 (从 {max_influenced_count} 降至 {current_influenced_count})。执行截断。")
                break  # 关键的截断操作

        print(f"截断后发送给前端的最终动画步数: {len(filtered_steps)}")
        # ====================================================================
        # 截断逻辑结束
        # ====================================================================

        # 3. 使用过滤后的 `filtered_steps` 来构建最终的JSON响应
        response_data = {
            "result_id": result_id,
            "total_steps": len(filtered_steps),  # total_steps 现在是过滤后的正确长度
            "simulation_steps": [
                {
                    "step": step.step,
                    "newly_activated_nodes": step.newly_activated_nodes,
                    "node_states": [
                        {"id": ns.id, "state": ns.state, "probability": ns.probability}
                        for ns in step.node_states
                    ]
                } for step in filtered_steps  # 遍历我们处理过的干净列表
            ]
        }
        
        print("动画数据生成并处理完毕。")
        return jsonify(response_data)
        
    except Exception as e:
        print(f"动画数据生成过程中发生错误: {e}")
        import traceback
        traceback.print_exc()
        return jsonify({"error": str(e)}), 500

        
# 【【【核心修改】】】用新的独立分析函数替换旧函数
@app.route('/api/influence/analysis/kl-core', methods=['POST'])
def run_kl_core_analysis():
    """
    从零开始，根据给定的参数和种子生成模式，运行(k,l)-core社区发现。
    不再需要 result_id。
    """
    json_data = request.get_json()
    if not json_data:
        return jsonify({"error": "Invalid JSON"}), 400

    try:
        # 从请求体中直接解析所有参数
        dataset_id = json_data.get("dataset_id")
        propagation_model = json_data.get("propagation_model")
        probability_model = json_data.get("probability_model")
        k_core = json_data.get("k_core")
        l_core = json_data.get("l_core")
        seed_budget = json_data.get("seed_budget", 10)
        seed_generation_mode = json_data.get("seed_generation_mode", "RANDOM")
        manual_seeds = json_data.get("seed_nodes", [])

        if not all([dataset_id, propagation_model, probability_model]) or k_core is None or l_core is None:
            return jsonify({"error": "Missing required parameters"}), 400
        
        print(f"开始独立 (k,l)-core 社区分析 (k={k_core}, l={l_core}, seed_mode={seed_generation_mode})...")

        # 调用新的、可以从零开始的C++函数
        community_analysis_result = imm_calculator.run_kl_core_analysis_from_scratch(
            dataset_id=dataset_id,
            propagation_model=propagation_model,
            probability_model=probability_model,
            k_core=k_core,
            l_core=l_core,
            seed_budget=seed_budget,
            seed_generation_mode=seed_generation_mode,
            manual_seeds=manual_seeds
        )

        # 封装和返回结果的逻辑不变
        response_data = {
            "result_id": community_analysis_result.result_id,
            "community": {
                "node_ids": community_analysis_result.community.node_ids,
                "average_influence_prob": community_analysis_result.community.average_influence_prob,
                "node_count": community_analysis_result.community.node_count
            },
            "message": community_analysis_result.message,
            "final_states": [ # <--- 新增部分
                {"id": ns.id, "state": ns.state, "probability": ns.probability}
                for ns in community_analysis_result.final_states
            ],
            "seed_nodes": community_analysis_result.seed_nodes
        }
        
        print("(k,l)-core 社区分析完成。")
        return jsonify(response_data)

    except Exception as e:
        import traceback
        print(f"社区分析过程中发生错误: {e}")
        traceback.print_exc()
        return jsonify({"error": str(e)}), 500

# 【【【核心修改】】】用新的独立分析函数替换旧函数
@app.route('/api/influence/analysis/k-core', methods=['POST'])
def run_k_core_analysis():
    """
    从零开始，根据给定的参数和种子生成模式，运行k-core社区发现。
    """
    json_data = request.get_json()
    if not json_data: return jsonify({"error": "Invalid JSON"}), 400

    try:
        dataset_id = json_data.get("dataset_id")
        propagation_model = json_data.get("propagation_model")
        probability_model = json_data.get("probability_model")
        k_core = json_data.get("k_core")
        seed_budget = json_data.get("seed_budget", 10)
        seed_generation_mode = json_data.get("seed_generation_mode", "RANDOM")
        manual_seeds = json_data.get("seed_nodes", [])

        if not all([dataset_id, propagation_model, probability_model]) or k_core is None:
            return jsonify({"error": "Missing required parameters."}), 400
        
        print(f"开始独立 k-core 社区分析 (k={k_core}, seed_mode={seed_generation_mode})...")

        # 调用新的C++ k-core 函数
        community_analysis_result = imm_calculator.run_k_core_analysis_from_scratch(
            dataset_id=dataset_id,
            propagation_model=propagation_model,
            probability_model=probability_model,
            k_core=k_core,
            seed_budget=seed_budget,
            seed_generation_mode=seed_generation_mode,
            manual_seeds=manual_seeds
        )

        response_data = {
            "result_id": community_analysis_result.result_id,
            "community": {
                "node_ids": community_analysis_result.community.node_ids,
                "average_influence_prob": community_analysis_result.community.average_influence_prob,
                "node_count": community_analysis_result.community.node_count
            },
            "message": community_analysis_result.message,
            "final_states": [ # <--- 新增部分
                {"id": ns.id, "state": ns.state, "probability": ns.probability}
                for ns in community_analysis_result.final_states
            ],
            "seed_nodes": community_analysis_result.seed_nodes
        }
        
        print("k-core 社区分析完成。")
        return jsonify(response_data)

    except Exception as e:
        import traceback
        print(f"k-core 社区分析过程中发生错误: {e}")
        traceback.print_exc()
        return jsonify({"error": str(e)}), 500

# 【【【核心修改】】】用新的独立分析函数替换旧函数
@app.route('/api/influence/analysis/k-truss', methods=['POST'])
def run_k_truss_analysis():
    """
    从零开始，根据给定的参数和种子生成模式，运行k-truss社区发现。
    """
    json_data = request.get_json()
    if not json_data: return jsonify({"error": "Invalid JSON"}), 400

    try:
        dataset_id = json_data.get("dataset_id")
        propagation_model = json_data.get("propagation_model")
        probability_model = json_data.get("probability_model")
        k_truss = json_data.get("k_truss")
        seed_budget = json_data.get("seed_budget", 10)
        seed_generation_mode = json_data.get("seed_generation_mode", "RANDOM")
        manual_seeds = json_data.get("seed_nodes", [])

        if not all([dataset_id, propagation_model, probability_model]) or k_truss is None:
            return jsonify({"error": "Missing required parameters."}), 400
        if k_truss < 2:
             return jsonify({"error": "k-truss 必须至少为 2"}), 400

        print(f"开始独立 k-truss 社区分析 (k={k_truss}, seed_mode={seed_generation_mode})...")

        # 调用新的C++ k-truss 函数
        community_analysis_result = imm_calculator.run_k_truss_analysis_from_scratch(
            dataset_id=dataset_id,
            propagation_model=propagation_model,
            probability_model=probability_model,
            k_truss=k_truss,
            seed_budget=seed_budget,
            seed_generation_mode=seed_generation_mode,
            manual_seeds=manual_seeds
        )

        response_data = {
            "result_id": community_analysis_result.result_id,
            "community": {
                "node_ids": community_analysis_result.community.node_ids,
                "average_influence_prob": community_analysis_result.community.average_influence_prob,
                "node_count": community_analysis_result.community.node_count
            },
            "message": community_analysis_result.message,
            "final_states": [ # <--- 新增部分
                {"id": ns.id, "state": ns.state, "probability": ns.probability}
                for ns in community_analysis_result.final_states
            ],
            "seed_nodes": community_analysis_result.seed_nodes
        }
        
        print("k-truss 社区分析完成。")
        return jsonify(response_data)

    except Exception as e:
        import traceback
        print(f"k-truss 社区分析过程中发生错误: {e}")
        traceback.print_exc()
        return jsonify({"error": str(e)}), 500

# ... (所有其他函数，如 get_blocking_animation_endpoint, run_critical_path_analysis 等保持不变) ...
# 【替换】现有的 get_blocking_animation_endpoint 函数
@app.route('/api/influence/blocking-animation', methods=['POST'])
def get_blocking_animation_endpoint():
    json_data = request.get_json()
    if not json_data: return jsonify({"error": "Invalid JSON"}), 400
    original_result_id = json_data.get("original_result_id")
    blocked_result_id = json_data.get("blocked_result_id")
    if not original_result_id or not blocked_result_id:
        return jsonify({"error": "Missing result IDs"}), 400

    try:
        # 从缓存中获取计算所需的参数
        if original_result_id not in computation_cache:
            return jsonify({"error": f"Result ID {original_result_id} not found"}), 404
        cached_data = computation_cache[original_result_id]
        
        if blocked_result_id not in computation_cache:
            return jsonify({"error": f"Result ID {blocked_result_id} not found"}), 404
        cached_data_after = computation_cache[blocked_result_id]

        # 调用新的C++核心函数
        animation_result = imm_calculator.get_blocking_animation(
            dataset_id=cached_data["dataset_id"],
            propagation_model=cached_data["propagation_model"],
            probability_model=cached_data["probability_model"],
            initial_nodes=cached_data.get("initial_nodes", []),
            blocking_nodes=cached_data_after.get("blocking_nodes", [])
        )

        # 转换并返回结果 (这部分逻辑不变)
        response_data = {
            "result_id": animation_result.result_id,
            "total_steps": animation_result.total_steps,
            "simulation_steps": [
                {
                    "step": step.step,
                    "newly_activated_nodes": step.newly_activated_nodes,
                    "newly_recovered_nodes": step.newly_recovered_nodes,
                    "node_states": [
                        {"id": ns.id, "state": ns.state, "probability": ns.probability}
                        for ns in step.node_states
                    ]
                } for step in animation_result.simulation_steps
            ]
        }
        return jsonify(response_data)
    except Exception as e:
        import traceback
        traceback.print_exc()
        return jsonify({"error": str(e)}), 500

# 【新增】将这个完整的API端点函数添加到 app.py 中
@app.route('/api/influence/analysis/critical-paths/<result_id>', methods=['POST'])
def run_critical_path_analysis(result_id):
    """
    根据给定的分析类型，找到关键传播路径。
    """
    if result_id not in computation_cache:
        return jsonify({"error": "Result ID not found or has expired."}), 404

    json_data = request.get_json()
    analysis_type = json_data.get("type")
    if analysis_type != "deepest":
        return jsonify({"error": "Invalid analysis type. Currently only 'deepest' is supported."}), 400

    try:
        cached_data = computation_cache[result_id]
        
        # 调用新的C++核心函数
        path_result = imm_calculator.find_critical_paths(
            result_id,
            cached_data["dataset_id"],
            cached_data["propagation_model"],
            cached_data["probability_model"],
            cached_data["initial_nodes"]
        )

        # 转换并返回结果
        response_data = {
            "result_id": path_result.result_id,
            "critical_paths": [
                {
                    "nodes": path.nodes,
                    "score": path.score,
                    "type": path.type
                }
                for path in path_result.critical_paths
            ],
            "message": path_result.message
        }
        return jsonify(response_data)

    except Exception as e:
        import traceback
        traceback.print_exc()
        return jsonify({"error": str(e)}), 500

# 【替换】现有的 calculate_influence_from_seeds 函数
@app.route('/api/influence/calculate-from-nodes', methods=['POST'])
def calculate_influence_from_nodes():
    """
    一个轻量级接口，接收一组种子节点和/或一组阻塞节点，并实时返回其影响力结果。
    用于最大化和最小化模式下的交互式选择。
    """
    json_data = request.get_json()
    if not json_data:
        return jsonify({"error": "Invalid JSON"}), 400

    try:
        # 从请求体中获取所有必要的参数
        dataset_id = json_data.get("dataset_id")
        propagation_model = json_data.get("propagation_model")
        probability_model = json_data.get("probability_model")
        
        # 种子节点和阻塞节点现在都是可选的
        seed_nodes = json_data.get("seed_nodes", [])
        blocking_nodes = json_data.get("blocking_nodes", [])

        if not all([dataset_id, propagation_model, probability_model]):
            return jsonify({"error": "Missing one or more required parameters (dataset_id, propagation_model, probability_model)."}), 400

        # 直接调用现有的C++核心函数进行计算
        result = imm_calculator.get_final_influence(
            dataset_id=dataset_id,
            propagation_model=propagation_model,
            probability_model=probability_model,
            initial_nodes=seed_nodes,
            blocking_nodes=blocking_nodes 
        )

        # 封装并返回与 /api/influence/final-state/<result_id> 格式一致的结果
        response_data = {
            "result_id": "interactive-result",
            "total_influence": result.total_influence,
            "final_states": [
                {"id": ns.id, "state": ns.state, "probability": ns.probability}
                for ns in result.final_states
            ]
        }
        return jsonify(response_data)

    except Exception as e:
        import traceback
        traceback.print_exc()
        return jsonify({"error": str(e)}), 500

if __name__ == '__main__':
    # 监听所有网络接口，端口为5001
    app.run(host='0.0.0.0', port=5019, debug=True)