/**
 * js/api.js
 * 
 * 负责所有与后端服务器的通信。
 * 每个函数都对应一个特定的 API 端点，封装了 fetch 调用、
 * 请求体构造和错误处理的细节。
 */

import { API_BASE_URL } from './config.js';

// 封装 fetch 以处理通用的错误情况
async function apiFetch(url, options) {
    const response = await fetch(url, options);
    if (!response.ok) {
        const errorText = await response.text();
        throw new Error(`API Error (${response.status}): ${errorText}`);
    }
    return response.json();
}

/**
 * 运行影响力分析（最大化或最小化）。
 * @param {object} requestBody - 发送给 /api/influence/run 的请求体。
 * @returns {Promise<object>} - 后端返回的分析结果数据。
 */
export function runInfluenceAnalysis(requestBody) {
    return apiFetch(`${API_BASE_URL}/api/influence/run`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(requestBody),
    });
}

/**
 * 在交互模式下，根据给定的种子/阻塞节点实时计算影响力。
 * @param {object} requestBody - 发送给 /api/influence/calculate-from-nodes 的请求体。
 * @returns {Promise<object>} - 后端返回的实时计算结果。
 */
export function calculateInfluenceFromNodes(requestBody) {
    return apiFetch(`${API_BASE_URL}/api/influence/calculate-from-nodes`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(requestBody),
    });
}


/**
 * 获取指定结果ID的最终影响力状态。
 * @param {string} resultId - 结果的唯一标识符。
 * @returns {Promise<object>} - 包含最终节点状态的数据。
 */
export function fetchFinalState(resultId) {
    return apiFetch(`${API_BASE_URL}/api/influence/final-state/${resultId}`);
}

/**
 * 获取指定结果ID的动画步骤数据。
 * @param {string} resultId - 结果的唯一标识符。
 * @returns {Promise<object>} - 包含模拟步骤的数据。
 */
export function fetchAnimationSteps(resultId) {
    return apiFetch(`${API_BASE_URL}/api/influence/step/${resultId}`);
}

/**
 * 获取最小化模式下的阻塞过程动画。
 * @param {string} originalResultId - 阻塞前的结果ID。
 * @param {string} blockedResultId - 阻塞后的结果ID。
 * @returns {Promise<object>} - 包含阻塞动画步骤的数据。
 */
export function fetchBlockingAnimation(originalResultId, blockedResultId) {
    return apiFetch(`${API_BASE_URL}/api/influence/blocking-animation`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
            original_result_id: originalResultId,
            blocked_result_id: blockedResultId
        }),
    });
}

/**
 * 【修改】运行 (k, l)-core 社区发现分析。
 * @param {object} requestBody - 包含所有分析参数的完整请求体。
 * @returns {Promise<object>} - 后端返回的社区信息。
 */
export function runCommunitySearch_KL(requestBody) {
    return apiFetch(`${API_BASE_URL}/api/influence/analysis/kl-core`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(requestBody),
    });
}

/**
 * 【修改】运行 k-core 社区发现分析。
 * @param {object} requestBody - 包含所有分析参数的完整请求体。
 * @returns {Promise<object>} - 后端返回的社区信息。
 */
export function runCommunitySearch_KCore(requestBody) {
    return apiFetch(`${API_BASE_URL}/api/influence/analysis/k-core`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(requestBody),
    });
}

/**
 * 【修改】运行 k-truss 社区发现分析。
 * @param {object} requestBody - 包含所有分析参数的完整请求体。
 * @returns {Promise<object>} - 后端返回的社区信息。
 */
export function runCommunitySearch_KTruss(requestBody) {
    return apiFetch(`${API_BASE_URL}/api/influence/analysis/k-truss`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(requestBody),
    });
}
// [!code-end]

/**
 * 运行关键传播链分析。
 * @param {string} resultId - 分析所基于的结果ID。
 * @returns {Promise<object>} - 后端返回的关键路径信息。
 */
export function runCriticalPathAnalysis(resultId) {
    return apiFetch(`${API_BASE_URL}/api/influence/analysis/critical-paths/${resultId}`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ type: "deepest" }), // 目前硬编码为 "deepest"
    });
}