/**
 * js/utils.js
 * 
 * 存放可重用的辅助函数。
 * 这些函数应该是“纯”的，不直接修改应用状态或DOM，
 * 使得它们易于测试和在项目中任何地方复用。
 */

/**
 * 将十六进制颜色字符串转换为 RGB 对象。
 * @param {string} hex - 十六进制颜色字符串 (例如, '#ff0000').
 * @returns {{r: number, g: number, b: number}|null} RGB对象或在格式错误时返回null。
 */
export function hexToRgb(hex) {
    const result = /^#?([a-f\d]{2})([a-f\d]{2})([a-f\d]{2})$/i.exec(hex);
    return result ? {
        r: parseInt(result[1], 16),
        g: parseInt(result[2], 16),
        b: parseInt(result[3], 16)
    } : null;
}

// js/utils.js

/**
 * 根据节点影响力概率的分布，生成一个颜色映射表。
 * 采用“混合百分位”算法，为指定的一组节点或所有激活的节点分配颜色。
 * 
 * @param {Map<string, {prob: number, state: string}>} probabilityMap - 包含所有节点状态和概率的完整 Map。
 * @param {object} palette - 当前的主题颜色对象 (e.g., state.currentPalette.community)。
 * @param {Set<string>|null} [targetNodeIds=null] - (可选) 一个包含节点ID的Set。如果提供，函数将只为这些ID的节点染色，忽略其active状态。如果为null，则按原逻辑为所有'active'节点染色。
 * @returns {Map<string, string>} 一个新的颜色映射 Map (nodeId -> colorString)。
 */
export function generateProportionalColorMap(probabilityMap, palette, targetNodeIds = null) {
    const newColorMap = new Map();

    // [!code-start]
    // 1. 【核心逻辑修改】根据是否提供了 targetNodeIds 来确定要染色的节点列表
    const nodesToColor = [];

    if (targetNodeIds) {
        // 社区搜索模式：遍历所有指定的社区节点ID
        for (const id of targetNodeIds) {
            // 无论节点状态是什么，都获取其概率值。如果节点不在概率图中，则默认概率为0。
            const data = probabilityMap.get(id) || { prob: 0, state: 'inactive' };
            nodesToColor.push({ id, prob: data.prob });
        }
    } else {
        // 最大化/最小化模式（原逻辑）：只筛选出状态为 'active' 的节点
        for (const [id, data] of probabilityMap.entries()) {
            if (data.state === 'active') {
                nodesToColor.push({ id, prob: data.prob });
            }
        }
    }

    if (nodesToColor.length === 0) {
        return newColorMap; // 如果没有需要染色的节点，返回空的 map
    }

    // 2. 按概率从低到高排序
    nodesToColor.sort((a, b) => a.prob - b.prob);

    // 3. 使用保底策略（如果节点数量太少）
    if (nodesToColor.length < 10) {
        const oneThirdIndex = Math.floor(nodesToColor.length / 3);
        const twoThirdsIndex = Math.floor(nodesToColor.length * 2 / 3);
        nodesToColor.forEach((node, index) => {
            if (index < oneThirdIndex) newColorMap.set(node.id, palette.cold);
            else if (index < twoThirdsIndex) newColorMap.set(node.id, palette.mid);
            else newColorMap.set(node.id, palette.hot);
        });
        return newColorMap;
    }

    // 4. 计算百分位阈值
    const p10Index = Math.floor(nodesToColor.length * 0.1);
    const p50Index = Math.floor(nodesToColor.length * 0.5);

    const coldThreshold = nodesToColor[p10Index].prob;
    const midThreshold = nodesToColor[p50Index].prob;

    // 5. 遍历所有目标节点，根据阈值分配颜色
    nodesToColor.forEach(node => {
        if (node.prob <= coldThreshold) {
            newColorMap.set(node.id, palette.cold);
        } else if (node.prob <= midThreshold) {
            newColorMap.set(node.id, palette.mid);
        } else {
            newColorMap.set(node.id, palette.hot);
        }
    });
    // [!code-end]

    return newColorMap;
}