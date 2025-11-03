/**
 * js/state.js
 * * 应用程序的中央状态管理。
 * 所有模块共享的可变状态都存储在这里。
 * 这使得状态的流动变得明确和可追溯。
 */

import { THEME_PALETTES } from './config.js';

// 导出一个单一的 state 对象，包含所有应用状态
export const state = {
    // 核心对象
    Graph: null, // 3D 图谱实例

    // 当前结果和数据
    probabilityMap: new Map(),        // 存储节点最终状态 {prob, state}
    proportionalColorMap: new Map(),  // 存储节点基于概率的颜色
    currentResultIds: {               // 存储后端返回的各种结果ID
        maximization: null,
        minimization_original: null,
        minimization_blocked: null
    },

    // 交互模式状态
    isInteractiveMode: false,
    interactiveSeedNodes: new Set(),
    interactiveBlockingNodes: new Set(),
    staticSeedNodesForMinimization: [], // 交互最小化模式下的“源”节点
    baselineInfluenceCount: 0,          // 交互最小化模式下的基线影响力计数
    baselineActiveNodeIds: new Set(),     // [!code ++] // 交互最小化模式下，基准状态的所有受影响节点
    interactiveSavedNodeIds: new Set(),   // [!code ++] // 交互最小化模式下，当前被挽救的节点
    debounceTimer: null,              // API防抖计时器

    // 动画播放状态
    animationInterval: null,
    animationSteps: [],
    currentStep: 0,
    isPlaying: false,
    currentAnimationType: null, // 'propagation' 或 'blocking'

    // 瞬时视觉效果状态 (用于动画高亮)
    newlyActivatedNodes: new Set(),
    newlyRecoveredNodes: new Set(),
    permanentlyRecoveredNodes: new Set(), // 阻塞动画中永久恢复的节点

    // 可视化高亮状态 (用于显示特定的路径或社区)
    specialNodeIds: new Set(),      // 种子/阻塞节点
    mainPropagationLinks: new Set(),
    criticalPathLinks: new Set(),
    cutOffLinks: new Set(),
    communitySearchActive: false,
    communityNodeIds: new Set(),
    isCommunityAggregated: false,
    communityColorMap: new Map(), // 存储社区内节点基于影响力的颜色

    // UI 和相机控制状态
    currentPalette: THEME_PALETTES.light,
    isCameraOrbiting: false,
    cameraOrbitInterval: null,
    currentMode: 'maximization',
};

/**
 * 重置与单次分析结果相关的状态。
 * 在每次点击 "执行分析" 时调用，以确保一个干净的开始。
 */
export function resetStateForNewAnalysis() {
    state.probabilityMap.clear();
    state.proportionalColorMap.clear();
    state.currentResultIds = { maximization: null, minimization_original: null, minimization_blocked: null };
    
    state.baselineInfluenceCount = 0;
    state.baselineActiveNodeIds.clear();   // [!code ++]
    state.interactiveSavedNodeIds.clear(); // [!code ++]

    // 重置动画状态
    if (state.animationInterval) clearTimeout(state.animationInterval);
    state.animationInterval = null;
    state.animationSteps = [];
    state.currentStep = 0;
    state.isPlaying = false;
    state.currentAnimationType = null;
    
    // 重置视觉高亮状态
    state.newlyActivatedNodes.clear();
    state.newlyRecoveredNodes.clear();
    state.permanentlyRecoveredNodes.clear();
    state.specialNodeIds.clear();
    state.mainPropagationLinks.clear();
    state.criticalPathLinks.clear();
    state.cutOffLinks.clear();
    state.communitySearchActive = false;
    state.communityNodeIds.clear();
    state.communityColorMap.clear();
    state.isCommunityAggregated = false;
}