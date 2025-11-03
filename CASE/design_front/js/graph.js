/**
 * js/graph.js
 * 
 * Manages the 3D graph visualization.
 * This module encapsulates all interactions with the ForceGraph3D library,
 * including initialization, data loading, and dynamic updates to nodes and links.
 */

import { state } from './state.js';
import * as UI from './ui.js';
import * as Api from './api.js';
import { generateProportionalColorMap } from './utils.js';

/**
 * Initializes the 3D graph.
 * @param {HTMLElement} element - The container element for the graph.
 * @param {string} initialDataUrl - The URL for the initial graph data.
 * @param {function} onNodeClickCallback - The function to call when a node is clicked.
 */
export function initialize(element, initialDataUrl, onNodeClickCallback) {
    state.Graph = ForceGraph3D()(element)
        .nodeLabel('id')
        .backgroundColor(state.currentPalette.background)
        .linkColor(() => state.currentPalette.inactivatedLink)
        .nodeColor(() => state.currentPalette.default)
        .onNodeClick(onNodeClickCallback)
        .onEngineStop(() => {
            console.log("Initial physics engine stopped. Centering graph...");
            state.Graph.zoomToFit(400, 100);
        });

    const resizeObserver = new ResizeObserver(entries => {
        const entry = entries[0];
        if (entry) {
            const { width, height } = entry.contentRect;
            state.Graph.width(width);
            state.Graph.height(height);
        }
    });
    resizeObserver.observe(element);
    
    loadData(initialDataUrl);
}

/**
 * Loads a new dataset into the graph.
 * @param {string} dataUrl - The URL of the new dataset to load.
 */
export function loadData(dataUrl) {
    if (!state.Graph) return;
    
    // 【核心修改】为URL添加一个时间戳参数来“破坏”缓存
    const urlWithCacheBuster = `${dataUrl}?t=${new Date().getTime()}`;

    fetch(urlWithCacheBuster) // 使用带有时间戳的新URL
        .then(res => res.json())
        .then(data => { state.Graph.graphData(data); })
        .catch(err => console.error('Error loading graph data:', err));
}

/**
 * Updates the visual appearance of all nodes based on the current state.
 */
export function updateNodeVisuals() {
    if (!state.Graph) return;

    state.Graph.nodeColor(node => {
        const nodeIdStr = String(node.id);
        
        if (state.newlyActivatedNodes.has(nodeIdStr)) return '#ffffff';
        if (state.newlyRecoveredNodes.has(nodeIdStr)) return '#00ff7f';

        if (state.specialNodeIds.has(nodeIdStr)) {
            // 在 CS 模式下, state.currentMode 是 'community_search',
            // 它不等于 'minimization', 所以会正确返回 seedNode 的颜色。
            return state.currentMode === 'minimization' 
                ? state.currentPalette.blockingNode 
                : state.currentPalette.seedNode;
        }
        
        // [!code-start]
        // 【核心修改】为交互模式下被挽救的节点着色
        if (state.isInteractiveMode && state.currentMode === 'minimization' && state.interactiveSavedNodeIds.has(nodeIdStr)) {
            return state.currentPalette.recovered; // 复用“已恢复”节点的颜色
        }
        // [!code-end]

        if (state.communitySearchActive) {
            if (state.communityNodeIds.has(nodeIdStr)) {
                // 如果节点在社区内，则从社区专属颜色地图获取颜色
                return state.communityColorMap.get(nodeIdStr) || state.currentPalette.inactive;
            }
            // 如果不在社区内，则为非活动状态
            return state.currentPalette.inactive;
        }
        if (state.permanentlyRecoveredNodes.has(nodeIdStr)) return state.currentPalette.recovered;
        if (state.proportionalColorMap.has(nodeIdStr)) return state.proportionalColorMap.get(nodeIdStr);
        return state.currentPalette.default;


    }).nodeVal(node => {
        const nodeIdStr = String(node.id);
        const DEFAULT_SIZE = 2;

        if (state.newlyActivatedNodes.has(nodeIdStr) || state.newlyRecoveredNodes.has(nodeIdStr)) return 25;
        if (state.specialNodeIds.has(nodeIdStr)) return 20;
        if (state.communitySearchActive) return state.communityNodeIds.has(nodeIdStr) ? 18 : DEFAULT_SIZE;
        if (state.permanentlyRecoveredNodes.has(nodeIdStr)) return DEFAULT_SIZE;

        const nodeInfo = state.probabilityMap.get(nodeIdStr);
        return (nodeInfo && nodeInfo.state === 'active') ? DEFAULT_SIZE + nodeInfo.prob * 15 : DEFAULT_SIZE;
    });
}



/**
 * Updates the visual appearance of all links based on the current state.
 */
export function updateLinkVisuals() {
    if (!state.Graph) return;

    state.Graph.linkColor(link => {
        const canonicalLinkId = `${Math.min(link.source.id, link.target.id)}-${Math.max(link.source.id, link.target.id)}`;
        if (state.criticalPathLinks.has(canonicalLinkId)) return state.currentPalette.criticalPath;
        if (state.mainPropagationLinks.has(canonicalLinkId)) return state.currentPalette.propagation;
        if (state.cutOffLinks.has(canonicalLinkId)) return state.currentPalette.cutOff;
        
        const sourceActive = state.probabilityMap.get(String(link.source.id))?.state === 'active';
        const targetActive = state.probabilityMap.get(String(link.target.id))?.state === 'active';
        return sourceActive && targetActive ? state.currentPalette.activatedLink : state.currentPalette.inactivatedLink;

    }).linkWidth(link => {
        const canonicalLinkId = `${Math.min(link.source.id, link.target.id)}-${Math.max(link.source.id, link.target.id)}`;
        if (state.criticalPathLinks.has(canonicalLinkId)) return 4.5;
        if (state.mainPropagationLinks.has(canonicalLinkId)) return 4;
        return state.cutOffLinks.has(canonicalLinkId) ? 3.5 : 1;
    }).linkDirectionalParticles(link => {
        const canonicalLinkId = `${Math.min(link.source.id, link.target.id)}-${Math.max(link.source.id, link.target.id)}`;
        if (state.cutOffLinks.has(canonicalLinkId)) return 0;
        if (state.mainPropagationLinks.has(canonicalLinkId)) return 4;
        
        const sourceActive = state.probabilityMap.get(String(link.source.id))?.state === 'active';
        const targetActive = state.probabilityMap.get(String(link.target.id))?.state === 'active';
        return sourceActive && targetActive ? 4 : 0;
    }).linkDirectionalParticleSpeed(0.01)
      .linkDirectionalParticleWidth(1.5)
      .linkDirectionalParticleColor(() => state.currentPalette.particle);
}

/**
 * Fetches and displays the final static state of a given result ID.
 */
export async function fetchAndDisplayFinalState(resultId) {
    if (!resultId) return;
    pauseAnimation();
    // UI.appendLog(`Visualizing final state for ID "${resultId}"...`);

    try {
        const data = await Api.fetchFinalState(resultId);
        state.probabilityMap = new Map(data.final_states.map(s => [String(s.id), { prob: s.probability, state: s.state }]));
        state.proportionalColorMap = generateProportionalColorMap(state.probabilityMap, state.currentPalette);
        
        updateNodeVisuals();
        updateLinkVisuals();
        // UI.appendLog(`Successfully visualized ID "${resultId}".`, 'success');
    } catch (error) {
        console.error('Failed to get final state:', error);
        // UI.appendLog(`Failed to get final state: ${error.message}`, 'error');
    }
}

// ===================================================================
// Animation Control Logic
// ===================================================================

/**
 * Initiates the process of fetching and starting an animation.
 */
export async function startAnimation(resultId, type, timelineContainer) {
    pauseAnimation();
    state.currentAnimationType = type;

    // Reset visualization state
    state.probabilityMap.clear();
    state.proportionalColorMap.clear();
    state.permanentlyRecoveredNodes.clear();
    state.specialNodeIds.forEach(id => {
        state.probabilityMap.set(String(id), { prob: 1.0, state: 'active' });
    });
    state.proportionalColorMap = generateProportionalColorMap(state.probabilityMap, state.currentPalette);
    updateNodeVisuals();
    updateLinkVisuals();
    
    // UI.appendLog('Loading animation data...');
    try {
        const data = type === 'blocking'
            ? await Api.fetchBlockingAnimation(state.currentResultIds.minimization_original, state.currentResultIds.minimization_blocked)
            : await Api.fetchAnimationSteps(resultId);
        
        state.animationSteps = data.simulation_steps;
        if (!state.animationSteps || state.animationSteps.length === 0) throw new Error("No animation steps available.");
        
        // [!code-start]
        // 【核心修改】在获取数据后，动态创建并设置Timeline
        UI.createTimelineControl(timelineContainer, state.animationSteps.length);
        setupTimelineEventListeners();
        // [!code-end]
        
        updateToStep(0); // 显示第一帧
        // UI.appendLog('Animation data loaded. Starting playback...', 'success');
        
        // playAnimation();
        // 
    } catch (error) {
        console.error('Failed to fetch animation data:', error);
        UI.appendLog(`Failed to fetch animation data: ${error.message}`, 'error');
        if (timelineContainer) timelineContainer.innerHTML = ''; // 清理容器
    }
}

/**
 * Wires up the event listeners for the dynamically created timeline controls.
 */
function setupTimelineEventListeners() {
    // 【注意】此函数不再需要导出，因为它只在 startAnimation 内部被调用
    const playPauseBtn = document.getElementById('play-pause-btn');

    if (playPauseBtn) {
        playPauseBtn.addEventListener('click', () => {
            if (state.isPlaying) {
                pauseAnimation();
            } else {
                playAnimation();
            }
            UI.setPlayButtonState(state.isPlaying);
        });
    }
}

/**
 * Updates the graph visualization to a specific step in the animation sequence.
 * @param {string | number} stepIndex - The index of the step to display.
 */
export function updateToStep(stepIndex) {
    state.currentStep = parseInt(stepIndex, 10);
    if (state.currentStep < 0 || state.currentStep >= state.animationSteps.length) return;

    const stepData = state.animationSteps[state.currentStep];
    
    state.probabilityMap.clear();
    state.specialNodeIds.forEach(id => {
        state.probabilityMap.set(String(id), { prob: 1.0, state: 'active' });
    });
    stepData.node_states?.forEach(ns => {
        state.probabilityMap.set(String(ns.id), { prob: ns.probability, state: ns.state });
    });
    state.proportionalColorMap = generateProportionalColorMap(state.probabilityMap, state.currentPalette);

    state.newlyActivatedNodes = new Set(stepData.newly_activated_nodes?.map(String) || []);
    state.newlyRecoveredNodes = new Set(stepData.newly_recovered_nodes?.map(String) || []);
    state.newlyRecoveredNodes.forEach(nodeId => state.permanentlyRecoveredNodes.add(nodeId));

    updateNodeVisuals();
    updateLinkVisuals();
    UI.updateTimelineUI(state.currentStep, state.animationSteps.length);

    setTimeout(() => {
        state.newlyActivatedNodes.clear();
        state.newlyRecoveredNodes.clear();
        updateNodeVisuals();
    }, 600);
}

/**
 * Starts the automatic playback of the animation.
 */
function playAnimation() {
    if (state.isPlaying) return;
    if (state.currentStep >= state.animationSteps.length - 1) {
        state.currentStep = -1;
    }
    state.isPlaying = true;

    function nextStep() {
        if (!state.isPlaying) return;
        state.currentStep++;
        if (state.currentStep >= state.animationSteps.length) {
            handleAnimationEnd();
            return;
        }
        updateToStep(state.currentStep);
        state.animationInterval = setTimeout(nextStep, 800);
    }
    nextStep();
}

/**
 * Pauses the automatic playback.
 */
export function pauseAnimation() {
    if (!state.isPlaying) return;
    state.isPlaying = false;
    clearTimeout(state.animationInterval);
}

/**
 * Handles the logic when the animation naturally finishes.
 */
function handleAnimationEnd() {
    pauseAnimation();
    UI.setPlayButtonState(false);
}

// --- Other Functions ---

/**
 * Focuses the camera on a specific node.
 * @param {string} nodeId - The ID of the node to highlight.
 */
export function highlightNode(nodeId) {
    if (!state.Graph) return;
    const node = state.Graph.graphData().nodes.find(n => String(n.id) === String(nodeId));
    if (!node) return;

    const distance = 150;
    const distRatio = 1 + distance / Math.hypot(node.x, node.y, node.z);
    const newPos = { x: node.x * distRatio, y: node.y * distRatio, z: node.z * distRatio };
    state.Graph.cameraPosition(newPos, node, 1000);
}

/**
 * Toggles the aggregation force for community nodes.
 * @param {boolean} isAggregated - Whether to aggregate the nodes.
 */
export function toggleCommunityAggregation(isAggregated) {
    state.isCommunityAggregated = isAggregated;

    if (state.isCommunityAggregated) {
        const communityNodes = state.Graph.graphData().nodes.filter(node => state.communityNodeIds.has(String(node.id)));
        if (communityNodes.length === 0) return;

        const center = {
            x: communityNodes.reduce((acc, node) => acc + node.x, 0) / communityNodes.length,
            y: communityNodes.reduce((acc, node) => acc + node.y, 0) / communityNodes.length,
            z: communityNodes.reduce((acc, node) => acc + node.z, 0) / communityNodes.length,
        };

        state.Graph.d3Force('x_community', d3.forceX(center.x).strength(d => state.communityNodeIds.has(String(d.id)) ? 0.5 : 0))
            .d3Force('y_community', d3.forceY(center.y).strength(d => state.communityNodeIds.has(String(d.id)) ? 0.5 : 0))
            .d3Force('z_community', d3.forceZ(center.z).strength(d => state.communityNodeIds.has(String(d.id)) ? 0.5 : 0));
    } else {
        removeCommunityForces();
    }
    state.Graph.d3ReheatSimulation();
}

/**
 * Removes the community aggregation forces.
 */
export function removeCommunityForces() {
     if(state.Graph) {
        state.Graph.d3Force('x_community', null)
            .d3Force('y_community', null)
            .d3Force('z_community', null);
     }
}

/**
 * Switches the graph view in minimization mode between before and after states.
 * @param {string} stateToShow - 'before' or 'after'.
 */
export function visualizeMinimizationState(stateToShow) {
    state.permanentlyRecoveredNodes.clear();
    UI.appendLog(''); 
    
    if (stateToShow === 'before') {
        state.specialNodeIds = new Set(state.staticSeedNodesForMinimization.map(String));
        state.cutOffLinks.clear();
        fetchAndDisplayFinalState(state.currentResultIds.minimization_original);
    } else { // 'after'
        state.specialNodeIds = new Set(Array.from(state.interactiveBlockingNodes).map(String));
        fetchAndDisplayFinalState(state.currentResultIds.minimization_blocked);
    }
}