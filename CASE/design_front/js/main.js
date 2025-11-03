/**
 * js/main.js
 * 
 * The main entry point and orchestrator for the application.
 * It initializes the app, sets up event listeners, and defines the high-level
 * control flow by calling functions from the other specialized modules.
 */

// Import modules
import * as Api from './api.js';
import * as Graph from './graph.js';
import * as UI from './ui.js';
import { state, resetStateForNewAnalysis } from './state.js';
import { generateProportionalColorMap } from './utils.js';
import { THEME_PALETTES } from './config.js';

// DOM Element References
const form = document.getElementById('control-form');
const resultsDiv = document.getElementById('results');
const datasetSelector = document.getElementById('dataset_id');
const dynamicControlsSection = document.getElementById('dynamic-controls-section');
const runButton = form.querySelector('.run-button');

// js/main.js

/**
 * Main application initialization logic.
 */
function initialize() {
    setupEventListeners();

    // [!code-start]
    // 初始化面板拖拽功能
    Split(['#control-panel-card', '#graph-card', '#results-card'], {
        sizes: [25, 50, 25], // 面板初始大小百分比
        minSize: [350, 400, 350], // 每个面板的最小宽度（像素）
        gutterSize: 15,      // 拖拽条的宽度，替代了原来的 gap
        snapOffset: 30,      // 拖拽到距离边缘30px时，自动贴合边缘
        cursor: 'col-resize' // 拖拽时的鼠标样式
    });
    // [!code-end]

    const prefersDark = window.matchMedia && window.matchMedia('(prefers-color-scheme: dark)').matches;
    setTheme(prefersDark ? 'dark' : 'light');

    const initialDataset = datasetSelector.value;
    const initialDataUrl = `../../${initialDataset}_subset_1000.json`;
    Graph.initialize(document.getElementById('3d-graph-container'), initialDataUrl, handleNodeClick);
    
    window.highlightNode = Graph.highlightNode;

    // [!code-start]
    // 传递当前模式来初始化图例
    UI.updateLegend(state.currentMode);
    // [!code-end]
    updateFormForMode('maximization');
}

// js/main.js

function setupEventListeners() {
    form.addEventListener('submit', handleFormSubmit);
    datasetSelector.addEventListener('change', handleDatasetChange);
    document.querySelectorAll('input[name="mode"]').forEach(radio => {
        radio.addEventListener('change', (e) => updateFormForMode(e.target.value));
    });
    // 【核心修改】监听所有社区算法单选按钮的 'change' 事件
    document.querySelectorAll('input[name="community_algorithm"]').forEach(radio => {
        radio.addEventListener('change', (e) => updateCommunityParams(e.target.value));
    });
}

/**
 * [!code-start]
 * 【新增】辅助函数：根据所选社区算法更新参数的可见性
 * @param {string} algorithm - 选中的算法 ('kl_core', 'k_core', 'k_truss')
 */
function updateCommunityParams(algorithm) {
    const kGroup = document.getElementById('k_param_group');
    const kLabel = document.getElementById('k_param_label');
    const lGroup = document.getElementById('l_param_group');

    if (algorithm === 'kl_core') {
        kGroup.classList.remove('hidden');
        kLabel.textContent = 'Param (K)';
        lGroup.classList.remove('hidden');
    } else if (algorithm === 'k_core') {
        kGroup.classList.remove('hidden');
        kLabel.textContent = 'Param (K)';
        lGroup.classList.add('hidden');
    } else if (algorithm === 'k_truss') {
        kGroup.classList.remove('hidden');
        kLabel.textContent = 'Param (K)';
        lGroup.classList.add('hidden');
    }
}

// 替换现有的 updateFormForMode 函数
function updateFormForMode(mode) {
    state.currentMode = mode;
    UI.updateLegend(mode); 
    const budgetGroup = document.getElementById('budget_group');
    const budgetLabel = document.getElementById('budget-label');
    const seedNodesGroup = document.getElementById('seed_nodes_group');
    const communityParamsGroup = document.getElementById('community_params_group');
    const negNumGroup = document.getElementById('neg_num_group');
    const minSeedModeGroup = document.getElementById('min_seed_mode_group'); // 【新增】获取新控件

    // 隐藏所有模式特定的组
    budgetGroup.classList.add('hidden');
    seedNodesGroup.classList.add('hidden');
    communityParamsGroup.classList.add('hidden');
    negNumGroup.classList.add('hidden');
    minSeedModeGroup.classList.add('hidden'); // 【新增】默认隐藏

    dynamicControlsSection.innerHTML = ''; 

    // 根据所选模式配置UI
    if (mode === 'maximization') {
        budgetGroup.classList.remove('hidden');
        budgetLabel.textContent = 'Seed Budget (K)';
        runButton.innerHTML = 'Run Influence Maximization';
        addInteractiveToggle();
    } else if (mode === 'minimization') {
        budgetGroup.classList.remove('hidden');
        budgetLabel.textContent = 'Blocking Budget (B)';
        negNumGroup.classList.remove('hidden');
        minSeedModeGroup.classList.remove('hidden'); // 【新增】显示最小化种子类型选择
        seedNodesGroup.classList.remove('hidden');
        runButton.innerHTML = 'Run Influence Minimization';
        addInteractiveToggle();
    } else if (mode === 'community_search') {
        budgetGroup.classList.remove('hidden');
        budgetLabel.textContent = 'Seed Budget (K)'; // CS模式现在也需要种子预算
        communityParamsGroup.classList.remove('hidden');
        runButton.innerHTML = 'Community Search';

        updateCommunityParams(document.querySelector('input[name="community_algorithm"]:checked').value);
    }
}

/**
 * Injects the interactive mode toggle and attaches its event listener.
 */
function addInteractiveToggle() {
    const toggleHTML = `
        <div class="control-sub-panel">
            <h4 class="sub-panel-title">🕹️ Interactive Mode</h4>
            <div class="control-row">
                <label for="interactive-mode-toggle">Enable Real-time Analysis</label>
                <label class="toggle-switch">
                    <input type="checkbox" id="interactive-mode-toggle">
                    <span class="slider"></span>
                </label>
            </div>
        </div>
    `;
    // Use insertAdjacentHTML to add to the container without overwriting it
    dynamicControlsSection.insertAdjacentHTML('beforeend', toggleHTML);
    document.getElementById('interactive-mode-toggle')
        .addEventListener('change', (e) => handleInteractiveModeChange(e.target.checked));
}

/**
 * Routes the form submission to the appropriate analysis function.
 * @param {Event} e - The form submission event.
 */
async function handleFormSubmit(e) {
    e.preventDefault();
    const mode = document.querySelector('input[name="mode"]:checked').value;

    runButton.disabled = true;
    runButton.textContent = '⏳ Calculating...';
    resultsDiv.innerHTML = 'Calculating, please wait...';
    
    resetStateForNewAnalysis();
    Graph.updateNodeVisuals();
    Graph.updateLinkVisuals();

    // Preserve the interactive toggle if it exists, but clear any old "Actions" panels
    const interactiveTogglePanel = dynamicControlsSection.querySelector('.control-sub-panel:has(#interactive-mode-toggle)');
    dynamicControlsSection.innerHTML = ''; // Clear previous actions
    if (interactiveTogglePanel) {
        dynamicControlsSection.appendChild(interactiveTogglePanel); // Add it back
    }

    try {
        if (mode === 'maximization' || mode === 'minimization') {
            await runInfluenceAnalysis(mode);
        } else if (mode === 'community_search') {
            await runCommunitySearchAnalysis();
        }
    } catch (error) {
        console.error('Analysis failed:', error);
        resultsDiv.innerHTML = `<p class="error"><b>Analysis Failed:</b> ${error.message}</p>`;
    } finally {
        runButton.disabled = false;
        const currentMode = document.querySelector('input[name="mode"]:checked').value;
        if (currentMode === 'maximization') runButton.innerHTML = 'Run Influence Maximization';
        else if (currentMode === 'minimization') runButton.innerHTML = 'Run Influence Minimization';
        else if (currentMode === 'community_search') runButton.innerHTML = 'Community Search';
    }
}

/**
 * Executes an influence analysis (Maximization or Minimization).
 * @param {string} mode - The analysis mode.
 */
async function runInfluenceAnalysis(mode) {
    const requestBody = {
        dataset_id: datasetSelector.value,
        mode: mode,
        params: {
            propagation_model: document.getElementById('propagation_model').value,
            budget: parseInt(document.getElementById('budget').value, 10),
            probability_model: document.getElementById('probability_model').value,
        }
    };
    if (mode === 'minimization') {
        requestBody.params.neg_num = parseInt(document.getElementById('neg_num').value, 10);
        requestBody.params.seed_generation_mode = document.querySelector('input[name="min_seed_mode"]:checked').value;
        const seedNodesRaw = document.getElementById('seed_nodes').value.trim();
        if (seedNodesRaw) {
            requestBody.params.seed_nodes = seedNodesRaw.split(',').map(s => parseInt(s.trim(), 10)).filter(n => !isNaN(n));
        }
    }

    // 1. Run the main analysis to get static results
    const data = await Api.runInfluenceAnalysis(requestBody);
    
    // 2. Based on the mode, handle the results and fetch animation data for charts
    if (mode === 'maximization' && data.result_id) {
        state.currentResultIds.maximization = data.result_id;
        state.specialNodeIds = new Set(data.seed_nodes.map(n => String(n.id)));
        state.mainPropagationLinks = new Set((data.main_propagation_paths || []).map(edge =>
            `${Math.min(edge.source, edge.target)}-${Math.max(edge.source, edge.target)}`
        ));
        
        await Graph.fetchAndDisplayFinalState(data.result_id);

        // Fetch propagation animation data for the line chart
        const animationData = await Api.fetchAnimationSteps(data.result_id);
        
        // Render results with BOTH static data and animation data
        // 【核心修改】在这里多传递一个 requestBody 参数
        UI.renderMaximizationResults(data, animationData.simulation_steps, requestBody);
        UI.addMaximizationActions(dynamicControlsSection, data.result_id);

    } else if (mode === 'minimization' && data.blocked_result_id) {
        state.currentResultIds.minimization_original = data.original_result_id;
        state.currentResultIds.minimization_blocked = data.blocked_result_id;
        state.staticSeedNodesForMinimization = data.seed_nodes.map(n => n.id);
        state.interactiveBlockingNodes = new Set(data.blocking_nodes.map(n => String(n.id))); // This will be used for specialNodeIds
        state.cutOffLinks = new Set((data.cut_off_paths || []).map(edge => 
            `${Math.min(edge.source, edge.target)}-${Math.max(edge.source, edge.target)}`
        ));
        
        state.specialNodeIds = new Set(data.blocking_nodes.map(n => String(n.id)));

        await Graph.fetchAndDisplayFinalState(data.blocked_result_id);
        
        // Fetch blocking animation data for the line chart
        const animationData = await Api.fetchBlockingAnimation(data.original_result_id, data.blocked_result_id);

        // Render results with BOTH static data and animation data
        UI.renderMinimizationResults(data, animationData.simulation_steps, requestBody);
        UI.addMinimizationActions(dynamicControlsSection, data);

    } else {
        throw new Error("Invalid API response format.");
    }
}

// js/main.js

// 替换现有的 runCommunitySearchAnalysis 函数
async function runCommunitySearchAnalysis() {
    // 1. 构建独立的社区发现请求体
    const algorithm = document.querySelector('input[name="community_algorithm"]:checked').value;
    // [!code-start]
    // 【核心修改】从新增的文本域读取手动输入的种子节点
    const csSeedNodesRaw = document.getElementById('cs_seed_nodes').value.trim();
    const requestBody = {
        dataset_id: datasetSelector.value,
        propagation_model: document.getElementById('propagation_model').value,
        probability_model: document.getElementById('probability_model').value,
        seed_budget: parseInt(document.getElementById('budget').value, 10),
        seed_generation_mode: document.querySelector('input[name="cs_seed_mode"]:checked').value,
        // 如果文本域不为空，则解析并使用其中的节点；否则，传递空数组
        seed_nodes: csSeedNodesRaw
            ? csSeedNodesRaw.split(',').map(s => parseInt(s.trim(), 10)).filter(n => !isNaN(n))
            : []
    };
    // [!code-end]

    // 2. 根据算法调用对应的API (这部分不变)
    let communityData;
    let communityRequestBodyForUI = {}; // 用于传递给UI渲染函数

    if (algorithm === 'kl_core') {
        requestBody.k_core = parseInt(document.getElementById('k_param').value, 10);
        requestBody.l_core = parseInt(document.getElementById('l_param').value, 10);
        communityRequestBodyForUI = { params: { k_core: requestBody.k_core, l_core: requestBody.l_core, algorithm: '(k, l)-core' } };
        communityData = await Api.runCommunitySearch_KL(requestBody);
    } else if (algorithm === 'k_core') {
        requestBody.k_core = parseInt(document.getElementById('k_param').value, 10);
        communityRequestBodyForUI = { params: { k_core: requestBody.k_core, algorithm: 'k-core' } };
        communityData = await Api.runCommunitySearch_KCore(requestBody);
    } else if (algorithm === 'k_truss') {
        requestBody.k_truss = parseInt(document.getElementById('k_param').value, 10);
        communityRequestBodyForUI = { params: { k_truss: requestBody.k_truss, algorithm: 'k-truss' } };
        communityData = await Api.runCommunitySearch_KTruss(requestBody);
    } else {
        throw new Error("Unknown community algorithm selected.");
    }

    // 3. 【【【核心修改】】】处理后端返回的完整数据
    // 将返回的 final_states 存入全局的 probabilityMap，这是所有颜色计算的基础
    state.probabilityMap = new Map((communityData.final_states || []).map(s => [String(s.id), { prob: s.probability, state: s.state }]));

    if (communityData.community && communityData.community.node_count > 0) {
        state.communityNodeIds = new Set(communityData.community.node_ids.map(String));
        state.communitySearchActive = true;

        // 【关键一步】现在 probabilityMap 有了数据，我们可以为社区节点生成专属的、有色彩层次的颜色图了
        state.communityColorMap = generateProportionalColorMap(
            state.probabilityMap,           // 1. 完整的概率图
            state.currentPalette.community,   // 2. 社区专属调色板
            state.communityNodeIds          // 3. 强制只为这些社区节点染色
        );

        // 将种子节点设为特殊节点以高亮显示
        // 注意：我们从 requestBody 中获取种子，因为独立模式下后端不返回它们
        state.specialNodeIds = new Set((communityData.seed_nodes || []).map(String));
        
    } else {
        // 如果未找到社区，清空所有相关状态
        state.communityNodeIds.clear();
        state.communitySearchActive = false;
        state.communityColorMap.clear();
        state.specialNodeIds.clear();
    }

    Graph.updateNodeVisuals(); // 触发图表重绘以应用新颜色

    // 4. 将完整数据传递给UI渲染函数
    UI.renderCommunitySearchResults(communityData, requestBody, communityRequestBodyForUI);
}


/**
 * Handles changes to the dataset selector.
 */
function handleDatasetChange() {
    const selectedDataset = datasetSelector.value;
    const dataUrl = `../../${selectedDataset}_subset_1000.json`;
    Graph.loadData(dataUrl);
    resultsDiv.innerHTML = 'Dataset changed. Please re-run analysis...';
    dynamicControlsSection.innerHTML = '';
    updateFormForMode(document.querySelector('input[name="mode"]:checked').value);
}


// ... (imports)

// ... (DOM Element References and other functions)

/**
 * Handles toggling the interactive analysis mode.
 */
async function handleInteractiveModeChange(enabled) {
    state.isInteractiveMode = enabled;
    
    // 清理状态
    state.probabilityMap.clear();
    state.interactiveSeedNodes.clear();
    state.interactiveBlockingNodes.clear();
    state.specialNodeIds.clear();
    state.baselineInfluenceCount = 0;
    state.baselineActiveNodeIds.clear();     // [!code ++]
    state.interactiveSavedNodeIds.clear(); // [!code ++]
    Graph.updateNodeVisuals();
    Graph.updateLinkVisuals();

    const formElementsToDisable = form.querySelectorAll('.control-sub-panel:not(:has(#interactive-mode-toggle)) input, .control-sub-panel:not(:has(#interactive-mode-toggle)) select, .control-sub-panel:not(:has(#interactive-mode-toggle)) textarea');

    if (!enabled) {
        runButton.disabled = false;
        formElementsToDisable.forEach(input => input.disabled = false);
        resultsDiv.innerHTML = 'Interactive mode disabled.';
        return;
    }

    runButton.disabled = true;
    formElementsToDisable.forEach(input => input.disabled = true);

    const currentMode = document.querySelector('input[name="mode"]:checked').value;
    if (currentMode === 'maximization') {
        resultsDiv.innerHTML = 'Click nodes in the graph to select seed nodes...';
    } else { // Minimization mode
        resultsDiv.innerHTML = 'Finding key influential nodes for simulation...';
        try {
            const maxRequest = {
                dataset_id: datasetSelector.value, mode: 'maximization',
                params: {
                    propagation_model: document.getElementById('propagation_model').value,
                    budget: parseInt(document.getElementById('budget').value, 10),
                    probability_model: document.getElementById('probability_model').value,
                }
            };
            const data = await Api.runInfluenceAnalysis(maxRequest);
            state.staticSeedNodesForMinimization = data.seed_nodes.map(n => n.id);
            
            if (data.result_id) {
                const finalStateData = await Api.fetchFinalState(data.result_id);
                // [!code-start]
                // 【核心修改】不仅计算基准数量，还要记录所有受影响节点的ID
                finalStateData.final_states.forEach(node => {
                    if (node.state === 'active') {
                        state.baselineActiveNodeIds.add(String(node.id));
                    }
                });
                state.baselineInfluenceCount = state.baselineActiveNodeIds.size;
                // [!code-end]
            }

            if (state.staticSeedNodesForMinimization.length === 0) throw new Error("Could not find any influential nodes to target.");
            
            await runInteractiveCalculation(); 
        } catch (error) {
            console.error('Failed to auto-find seed nodes for minimization:', error);
            resultsDiv.innerHTML = `<p class="error"><b>Initialization Failed:</b> ${error.message}</p>`;
            document.getElementById('interactive-mode-toggle').checked = false;
            handleInteractiveModeChange(false);
        }
    }
}

// ... (handleNodeClick)

/**
 * Runs the real-time calculation for interactive mode.
 */
async function runInteractiveCalculation() {
    if (!state.isInteractiveMode) return;
    const currentMode = document.querySelector('input[name="mode"]:checked').value;
    const requestBody = {
        dataset_id: datasetSelector.value,
        propagation_model: document.getElementById('propagation_model').value,
        probability_model: document.getElementById('probability_model').value,
    };

    if (currentMode === 'maximization') {
        if (state.interactiveSeedNodes.size === 0) {
            state.probabilityMap.clear();
            state.interactiveSavedNodeIds.clear(); // [!code ++]
            Graph.updateNodeVisuals();
            UI.renderFullInteractiveMaximizationResults();
            return;
        }
        requestBody.seed_nodes = Array.from(state.interactiveSeedNodes).map(Number);
        requestBody.blocking_nodes = [];
    } else { // Minimization
        requestBody.seed_nodes = state.staticSeedNodesForMinimization;
        requestBody.blocking_nodes = Array.from(state.interactiveBlockingNodes).map(Number);
    }

    resultsDiv.innerHTML = 'Calculating in real-time...';

    try {
        const data = await Api.calculateInfluenceFromNodes(requestBody);
        state.probabilityMap = new Map(data.final_states.map(s => [String(s.id), { prob: s.probability, state: s.state }]));
        state.proportionalColorMap = generateProportionalColorMap(state.probabilityMap, state.currentPalette);
        
        // [!code-start]
        // 【核心修改】计算当前被挽救的节点
        if (currentMode === 'minimization') {
            state.interactiveSavedNodeIds.clear();
            for (const nodeId of state.baselineActiveNodeIds) {
                // 如果一个节点在基准中是 active，但在当前状态图中不是 active，则它被挽救了
                const currentNodeState = state.probabilityMap.get(nodeId);
                if (!currentNodeState || currentNodeState.state !== 'active') {
                    state.interactiveSavedNodeIds.add(nodeId);
                }
            }
        }
        // [!code-end]

        Graph.updateNodeVisuals();
        Graph.updateLinkVisuals();
        
        currentMode === 'maximization' 
            ? UI.renderFullInteractiveMaximizationResults() 
            : UI.renderFullInteractiveMinimizationResults();
    } catch (error) {
        console.error('Interactive calculation failed:', error);
        resultsDiv.innerHTML = `<p class="error"><b>Calculation Failed:</b> ${error.message}</p>`;
    }
}


// ... (setTheme, initialize)

/**
 * Handles a click event on a graph node for interactive mode.
 */
function handleNodeClick(node) {
    if (!state.isInteractiveMode) return;
    const nodeId = String(node.id);
    const currentMode = document.querySelector('input[name="mode"]:checked').value;

    if (currentMode === 'maximization') {
        state.interactiveSeedNodes.has(nodeId) ? state.interactiveSeedNodes.delete(nodeId) : state.interactiveSeedNodes.add(nodeId);
        state.specialNodeIds = new Set(state.interactiveSeedNodes);
    } else { // Minimization
        state.interactiveBlockingNodes.has(nodeId) ? state.interactiveBlockingNodes.delete(nodeId) : state.interactiveBlockingNodes.add(nodeId);
        state.specialNodeIds = new Set(state.interactiveBlockingNodes);
    }
    
    Graph.updateNodeVisuals();
    clearTimeout(state.debounceTimer);
    state.debounceTimer = setTimeout(runInteractiveCalculation, 500);
}


/**
 * Sets the application theme (light or dark).
 * @param {'light' | 'dark'} themeName - The name of the theme to apply.
 */
function setTheme(themeName) {
    // [!code-start]
    // 【核心修改】如果请求的主题不存在，就默认使用 'light'
    const effectiveThemeName = THEME_PALETTES[themeName] ? themeName : 'light';
    
    document.body.classList.toggle('dark-theme', effectiveThemeName === 'dark');
    state.currentPalette = THEME_PALETTES[effectiveThemeName];
    // [!code-end]
    
    if (state.Graph) {
        state.Graph.backgroundColor(state.currentPalette.background);
        state.proportionalColorMap = generateProportionalColorMap(state.probabilityMap, state.currentPalette);
        UI.updateLegend(); // 注意：这里可能需要传参，根据您的最新代码调整
        Graph.updateNodeVisuals();
        Graph.updateLinkVisuals();
    }
}

// --- App Entry Point ---
document.addEventListener('DOMContentLoaded', initialize);