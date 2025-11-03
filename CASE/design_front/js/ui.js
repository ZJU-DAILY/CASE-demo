import { state } from './state.js';
import * as Graph from './graph.js';
import * as Api from './api.js';
import { THEME_PALETTES } from './config.js';

// DOM Element References
const resultsDiv = document.getElementById('results');
const legendContainer = document.getElementById('legend-items-container');

// js/ui.js

export function updateLegend(mode) {
    if (!legendContainer) return;

    // [!code-start]
    // 1. Define mode-specific legend items
    const maximizationItems = [
        { label: 'High Influenced Node', key: 'hot', type: 'node' },
        { label: 'Medium Influenced Node', key: 'mid', type: 'node' },
        { label: 'Low Influenced Node', key: 'cold', type: 'node' },
        { label: 'Seed Node', key: 'seedNode', type: 'node' },
        { label: 'Main Propagation Path', key: 'propagation', type: 'line' },
        { label: 'Critical Chain', key: 'criticalPath', type: 'line' },
    ];

    const minimizationItems = [
        { label: 'High Influenced Node', key: 'hot', type: 'node' },
        { label: 'Medium Influenced Node', key: 'mid', type: 'node' },
        { label: 'Low Influenced Node', key: 'cold', type: 'node' },
        { label: 'Blocking Node', key: 'blockingNode', type: 'node' },
        { label: 'Recovered Node', key: 'recovered', type: 'node' },
        { label: 'Cut-off Path', key: 'cutOff', type: 'line' },
    ];

    const communitySearchItems = [
        { label: 'High Influenced Node', key: 'community.hot', type: 'node' },
        { label: 'Medium Influenced Node', key: 'community.mid', type: 'node' },
        { label: 'Low Influenced Node', key: 'community.cold', type: 'node' },
        { label: 'Seed Node', key: 'seedNode', type: 'node' },
        { label: 'Inactive Node', key: 'inactive', type: 'node' },
    ];

    // 2. Select the correct mapping based on the current mode
    let legendMapping;
    switch (mode) {
        case 'maximization':
            legendMapping = maximizationItems;
            break;
        case 'minimization':
            legendMapping = minimizationItems;
            break;
        case 'community_search':
            legendMapping = communitySearchItems;
            break;
        default:
            legendMapping = maximizationItems; // Default fallback
    }

    // 3. The rest of the function remains the same, it just uses the selected mapping
    legendContainer.innerHTML = '';

    legendMapping.forEach(item => {
        let color;
        // This logic handles nested keys like 'community.hot'
        if (item.key.includes('.')) {
            const [paletteKey, colorKey] = item.key.split('.');
            color = state.currentPalette[paletteKey]?.[colorKey];
        } else {
            color = state.currentPalette[item.key];
        }

        if (!color) return; // Skip if color not found

        const legendItem = document.createElement('div');
        legendItem.className = 'legend-item';
        const swatchHtml = item.type === 'line'
            ? `<div style="width: 16px; height: 4px; border-radius: 2px; margin-right: 10px; background-color: ${color}; align-self: center;"></div>`
            : `<div class="legend-swatch" style="background-color: ${color};"></div>`;
        legendItem.innerHTML = `${swatchHtml}<span>${item.label}</span>`;
        legendContainer.appendChild(legendItem);
    });
    // [!code-end]
}

// js/ui.js

export function renderMaximizationResults(data, simulationSteps, requestBody) {
    // 1. Prepare pie chart data & calculate influenced stats
    let highCount = 0, midCount = 0, lowCount = 0;

    // [!code-start]
    // 【核心修复】使用正确的关键字 'seedNode' 来获取颜色
    const { hot, mid, cold, seedNode, inactive } = state.currentPalette;
    // [!code-end]

    for (const color of state.proportionalColorMap.values()) {
        if (color === hot) highCount++;
        else if (color === mid) midCount++;
        else if (color === cold) lowCount++;
    }
    const seedCount = data.seed_nodes.length;
    const influencedCount = highCount + midCount + lowCount + seedCount;
    const totalNodes = state.Graph.graphData().nodes.length;
    const influenceRatio = totalNodes > 0 ? (influencedCount / totalNodes * 100).toFixed(1) : 0;
    const uninfluencedCount = totalNodes - influencedCount;

    // 2. Prepare NEW parameter tags (不变)
  /* const params = [
        { label: 'Propagation Model', value: requestBody.params.propagation_model },
        { label: 'Probability Model', value: requestBody.params.probability_model },
    ];
    let tagsHtml = `<div class="summary-tags-container">${params.map(p => `<div class="summary-tag"><span class="label">${p.label}</span><span class="value">${p.value}</span></div>`).join('')}</div>`; */
    const keyMetricsHtml = `
        <h4 class="result-section-title">⭐ Key Metrics</h4>
        <div class="key-metrics-container">
            <div class="key-metric-item">
                <span class="metric-icon">🎯</span>
                <div class="metric-text">
                    <span class="metric-label">Total Influenced Nodes</span>
                    <span class="metric-value">${influencedCount.toLocaleString()} (${influenceRatio}%)</span>
                </div>
            </div>
        </div>
    `;

    // 3. Prepare pie data for the chart (不变)
    const pieData = [
        { value: highCount, name: 'High Influenced' },
        { value: midCount, name: 'Medium Influenced' },
        { value: lowCount, name: 'Low Influenced' },
        { value: seedCount, name: 'Seed Nodes' },
        { value: uninfluencedCount, name: 'Uninfluenced' }
    ];

    // [!code-start]
    // 【核心修复】确保颜色数组中传入的是正确的 'seedNode' 颜色
    const pieColors = [hot, mid, cold, seedNode, inactive];
    // [!code-end]

    // 4. Render HTML
    resultsDiv.innerHTML = `
    ${keyMetricsHtml} 
    <div class="result-section"><h4>🌱 Seed Nodes</h4><div class="node-chip-list">${data.seed_nodes.map(n => `<div class="node-chip" onclick="window.highlightNode('${n.id}')">${n.id}</div>`).join('')}</div></div>
    <h4 class="chart-title">📊 Influence Distribution</h4>
    <div id="max-pie-chart" class="chart-container"></div>
    <h4 class="chart-title">📉 Propagation Timeline</h4>
    <div id="max-line-chart" class="chart-container"></div>
    <div id="log-messages"></div>
    <div id="critical-path-results"></div> 
`;


    // 5. Initialize charts
    initPieChart('max-pie-chart', 'Influence Distribution', pieData, pieColors);
    if (simulationSteps && simulationSteps.length > 0) {
        initLineChart('max-line-chart', 'Propagation Timeline', simulationSteps, 'maximization');
    } else {
        document.getElementById('max-line-chart').innerHTML = '<p class="info-text" style="text-align:center;">Could not generate timeline chart.</p>';
    }
}

/**
 * Adds the post-analysis action controls for Maximization to the left panel.
 * @param {HTMLElement} container - The container for the controls.
 * @param {string} resultId - The ID of the analysis result.
 */
export function addMaximizationActions(container, resultId) {
    const actionsPanel = document.createElement('div');
    actionsPanel.className = 'control-sub-panel';
    actionsPanel.id = 'dynamic-actions-panel';
    actionsPanel.innerHTML = `
        <h4 class="sub-panel-title">⚡ Actions</h4>
        <div class="form-group" style="display: flex; flex-direction: column; gap: 10px;">
             <button class="action-button" id="play-propagation-btn">Play Animation</button>
             <button class="action-button" id="run-critical-path-btn">Find Critical Chain</button>
        </div>
        <div id="timeline-container"></div>
    `;
    container.appendChild(actionsPanel);

    document.getElementById('play-propagation-btn').onclick = async (e) => {
        const btn = e.target;
        btn.disabled = true;
        btn.textContent = '⏳ Loading Animation...';

        // 1. 加载动画数据并创建时间轴 (这也会启动播放)
        await Graph.startAnimation(resultId, 'propagation', document.getElementById('timeline-container'));
        
        // [!code-start]
        // 2. 【核心修改】立即暂停动画，等待用户操作
        // Graph.pauseAnimation(); // 假设 graph.js 提供了此函数
        
        // 3. 更新加载按钮的文本
        btn.textContent = '✅ Animation Loaded';
        
        // 4. 确保 "Play" 按钮显示为 "Play" (而不是 "Pause")
        // setPlayButtonState(false);

        // 5. 为我们新的 "Step" 按钮附加监听器
        const stepBtn = document.getElementById('step-forward-btn');
        if (stepBtn) {
            stepBtn.onclick = handleStepForward;
        }
        // [!code-end]
    };

    document.getElementById('run-critical-path-btn').onclick = (e) => handleCriticalPathAnalysis(resultId, e.target);
}
export function renderMinimizationResults(data, simulationSteps, requestBody) {
    // 1. Calculate saved stats
    const savedCount = data.influence_before.count - data.influence_after.count;
    const totalNodes = state.Graph.graphData().nodes.length;
    const savedRatio = totalNodes > 0 ? (savedCount / data.influence_before.count * 100).toFixed(1) : 0;

    // 2. Prepare NEW parameter tags
    // const params = [
    //     { label: 'Propagation Model', value: requestBody.params.propagation_model },
    //     { label: 'Probability Model', value: requestBody.params.probability_model }, // <-- 新增这一行
    // ];
    // let tagsHtml = `<div class="summary-tags-container">${params.map(p => `<div class="summary-tag"><span class="label">${p.label}</span><span class="value">${p.value}</span></div>`).join('')}</div>`;

    const keyMetricsHtml = `
        <h4 class="result-section-title">⭐ Key Metrics</h4>
        <div class="key-metrics-container">
            <div class="key-metric-item">
                <span class="metric-icon">🛡️</span>
                <div class="metric-text">
                    <span class="metric-label">Nodes Saved from Influence</span>
                    <span class="metric-value">${savedCount.toLocaleString()} (${savedRatio}%)</span>
                </div>
            </div>
        </div>
    `;

    // 3. Prepare pie chart data
    const remainingInfluenced = data.influence_after.count;
    const unaffected = totalNodes - data.influence_before.count;

    const pieData = [
        { value: savedCount, name: 'Saved Nodes' },
        { value: remainingInfluenced, name: 'Still Influenced' },
        { value: unaffected, name: 'Uninfluenced' }
    ];
    const pieColors = [state.currentPalette.recovered, state.currentPalette.hot, state.currentPalette.inactive];

    // 4. Render HTML
    resultsDiv.innerHTML = `
        ${keyMetricsHtml}
        <div class="result-section"><h4>🛡️ Blocking Nodes</h4><div class="node-chip-list">${data.blocking_nodes.map(n => `<div class="node-chip" onclick="window.highlightNode('${n.id}')">${n.id}</div>`).join('')}</div></div>
        <h4 class="chart-title">📊 Blocking Effectiveness</h4>
        <div id="min-pie-chart" class="chart-container"></div>
        <h4 class="chart-title">📉 Saved Nodes Timeline</h4>
        <div id="min-line-chart" class="chart-container"></div>
        <div id="log-messages"></div>`;
    // 5. Initialize charts
    initPieChart('min-pie-chart', 'Blocking Effectiveness', pieData, pieColors);
    if (simulationSteps && simulationSteps.length > 0) {
        initLineChart('min-line-chart', 'Saved Nodes Timeline', simulationSteps, 'minimization');
    } else {
        document.getElementById('min-line-chart').innerHTML = '<p class="info-text" style="text-align:center;">Could not generate timeline chart.</p>';
    }

    Graph.visualizeMinimizationState('after');
}

export function addMinimizationActions(container) {
    const actionsPanel = document.createElement('div');
    actionsPanel.className = 'control-sub-panel';
    actionsPanel.id = 'dynamic-actions-panel';
    actionsPanel.innerHTML = `
        <h4 class="sub-panel-title">👁️ Actions & Visualization</h4>
        <div class="form-group inline">
            <label>View State</label>
            <div class="radio-group" style="flex-grow: 1;">
                <label class="radio-label"><input type="radio" name="min-view" value="before">Before</label>
                <label class="radio-label"><input type="radio" name="min-view" value="after" checked>After</label>
            </div>
        </div>
        <div class="form-group" style="display: flex; flex-direction: column; gap: 10px;">
            <button class="action-button" id="play-blocking-animation-btn">Play Blocking Animation</button>
        </div>
        <div id="timeline-container"></div>
    `;
    container.appendChild(actionsPanel);

    document.querySelectorAll('input[name="min-view"]').forEach(radio => {
        radio.addEventListener('change', e => Graph.visualizeMinimizationState(e.target.value));
    });

    document.getElementById('play-blocking-animation-btn').addEventListener('click', async (e) => {
        const btn = e.target;
        btn.disabled = true;
        btn.textContent = '⏳ Loading Animation...';

        // 1. 加载动画数据并创建时间轴 (这也会启动播放)
        await Graph.startAnimation(null, 'blocking', document.getElementById('timeline-container'));
        
        // [!code-start]
        // 2. 【核心修改】立即暂停动画，等待用户操作
        // Graph.pauseAnimation(); // 假设 graph.js 提供了此函数

        // 3. 更新加载按钮的文本
        btn.textContent = '✅ Animation Loaded';
        
        // 4. 确保 "Play" 按钮显示为 "Play" (而不是 "Pause")
        // setPlayButtonState(false);

        // 5. 为我们新的 "Step" 按钮附加监听器
        const stepBtn = document.getElementById('step-forward-btn');
        if (stepBtn) {
            stepBtn.onclick = handleStepForward;
        }
        // [!code-end]
    });
}
// js/ui.js

// 替换现有的 renderCommunitySearchResults 函数
export function renderCommunitySearchResults(communityData, originalRequestBody, communityParamsForUI) {
    // 1. 计算社区统计数据 (不变)
    const communityNodesCount = communityData.community?.node_count || 0;
    const totalNodes = state.Graph.graphData().nodes.length;
    const communityRatio = totalNodes > 0 ? (communityNodesCount / totalNodes * 100).toFixed(1) : 0;
    const avgProb = communityData.community?.average_influence_prob || 0;
    const avgProbFormatted = (avgProb * 100).toFixed(2);
    const algorithmName = communityParamsForUI.params.algorithm || 'Community';

    const keyMetricsHtml = `
        <h4 class="result-section-title">⭐ Key Metrics (${algorithmName})</h4>
        <div class="key-metrics-container">
            <div class="key-metric-item">
                <span class="metric-icon">👥</span>
                <div class="metric-text">
                    <span class="metric-label">Community Nodes</span>
                    <span class="metric-value">${communityNodesCount.toLocaleString()}</span>
                </div>
            </div>
            <div class="key-metric-item">
                <span class="metric-icon">📈</span>
                <div class="metric-text">
                    <span class="metric-label">Graph Percentage</span>
                    <span class="metric-value">${communityRatio}%</span>
                </div>
            </div>
            <div class="key-metric-item">
                <span class="metric-icon">📊</span>
                <div class="metric-text">
                    <span class="metric-label">Average Influence Probability</span>
                    <span class="metric-value">${avgProbFormatted}%</span>
                </div>
            </div>
        </div>
    `;

    let seedNodesHtml = '';
    const seedNodes = communityData.seed_nodes || [];
    if (seedNodes.length > 0) {
        seedNodesHtml = `
            <div class="result-section">
                <h4>🌱 Seed Nodes (${seedNodes.length})</h4>
                <div class="node-chip-list">
                    ${seedNodes.map(id => `<div class="node-chip" onclick="window.highlightNode('${id}')">${id}</div>`).join('')}
                </div>
            </div>`;
    }

    // 2. 【【【核心修改】】】恢复种子节点/参数的显示逻辑
    // 3. 【【【核心修改】】】恢复饼图逻辑
    let highInfluenceInCommunity = 0;
    let midInfluenceInCommunity = 0;
    let lowInfluenceInCommunity = 0;

    const { hot, mid, cold } = state.currentPalette.community;
    const { inactive } = state.currentPalette;

    if (communityNodesCount > 0) {
        for (const nodeId of state.communityNodeIds) {
            const color = state.communityColorMap.get(nodeId);
            if (color === hot) highInfluenceInCommunity++;
            else if (color === mid) midInfluenceInCommunity++;
            else if (color === cold) lowInfluenceInCommunity++;
        }
    }
    const outsideNodesCount = totalNodes - communityNodesCount;

    const pieData = communityNodesCount > 0 ? [
        { value: highInfluenceInCommunity, name: 'High-Influenced' },
        { value: midInfluenceInCommunity, name: 'Mid-Influenced' },
        { value: lowInfluenceInCommunity, name: 'Low-Influenced' },
        { value: outsideNodesCount, name: 'Outside Community' }
    ] : [];
    const pieColors = [hot, mid, cold, inactive];

    // 4. 渲染最终HTML
    resultsDiv.innerHTML = `
        ${keyMetricsHtml}
        ${seedNodesHtml} 
        ${communityNodesCount > 0 ? `<h4 class="chart-title">📊 Community Composition Analysis</h4><div id="cs-pie-chart" class="chart-container"></div>` : `<p class="info-text">${communityData.message || 'No community found.'}</p>`}
        <div id="log-messages"></div>
    `;

    // 5. 初始化饼图
    if (communityNodesCount > 0) {
        initPieChart('cs-pie-chart', 'Community Composition', pieData, pieColors);
    }
    
    // 6. 添加聚合控制 (不变)
    const dynamicControlsContainer = document.getElementById('dynamic-controls-section');
    if (communityData.community?.node_count > 0) {
        const communityPanel = document.createElement('div');
        communityPanel.className = 'control-sub-panel';
        communityPanel.id = 'dynamic-actions-panel';
        communityPanel.innerHTML = `
            <h4 class="sub-panel-title">👀Visualization</h4>
            <div class="control-row">
                <label>Aggregate Community</label>
                <label class="toggle-switch">
                    <input type="checkbox" id="community-aggregate-toggle">
                    <span class="slider"></span>
                </label>
            </div>`;
        dynamicControlsContainer.innerHTML = '';
        dynamicControlsContainer.appendChild(communityPanel);
        document.getElementById('community-aggregate-toggle').addEventListener('change', e => {
            Graph.toggleCommunityAggregation(e.target.checked);
        });
    } else {
        dynamicControlsContainer.innerHTML = '';
    }
}


/**
 * Renders the results for interactive 'maximization' mode.
 */
export function renderInteractiveMaximizationResults() {
    const influenceCount = Array.from(state.probabilityMap.values()).filter(v => v.state === 'active').length;
    resultsDiv.innerHTML = `
        <div class="result-section">
            <h4>🖱️ Interactive Seeds (${state.interactiveSeedNodes.size})</h4>
            <div class="node-chip-list">${[...state.interactiveSeedNodes].map(id => `<div class="node-chip" onclick="window.highlightNode('${id}')">${id}</div>`).join('')}</div>
        </div>
        <div class="result-section"><h4>📈 Real-time Influence</h4><div class="metric"><span class="label">Influenced Nodes</span><span class="value">${influenceCount.toLocaleString()}</span></div></div>
        <div id="log-messages" class="info-text">${state.interactiveSeedNodes.size > 0 ? '' : 'Click nodes to select seeds.'}</div>`;
}

/**
 * Renders the results for interactive 'maximization' mode with a full chart display.
 */
export function renderFullInteractiveMaximizationResults() {
    // 如果没有选择种子节点，显示初始提示信息
    if (state.interactiveSeedNodes.size === 0) {
        resultsDiv.innerHTML = `<div id="log-messages" class="info-text">Click nodes in the graph to select seed nodes.</div>`;
        return;
    }

    // 1. 计算饼图所需数据
    const { hot, mid, cold, seedNode, inactive } = state.currentPalette;
    let highCount = 0, midCount = 0, lowCount = 0;

    for (const color of state.proportionalColorMap.values()) {
        if (color === hot) highCount++;
        else if (color === mid) midCount++;
        else if (color === cold) lowCount++;
    }
    const seedCount = state.interactiveSeedNodes.size;
    const influencedCount = highCount + midCount + lowCount + seedCount;
    const totalNodes = state.Graph.graphData().nodes.length;
    const influenceRatio = totalNodes > 0 ? (influencedCount / totalNodes * 100).toFixed(1) : 0;
    const uninfluencedCount = totalNodes - influencedCount;

    // 2. 准备关键指标和节点列表的HTML
    const keyMetricsHtml = `
        <h4 class="result-section-title">⭐ Key Metrics</h4>
        <div class="key-metrics-container">
            <div class="key-metric-item">
                <span class="metric-icon">🎯</span>
                <div class="metric-text">
                    <span class="metric-label">Total Influenced Nodes</span>
                    <span class="metric-value">${influencedCount.toLocaleString()} (${influenceRatio}%)</span>
                </div>
            </div>
        </div>
    `;
    const seedNodesHtml = `
        <div class="result-section">
            <h4>🌱 Interactive Seeds (${seedCount})</h4>
            <div class="node-chip-list">${[...state.interactiveSeedNodes].map(id => `<div class="node-chip" onclick="window.highlightNode('${id}')">${id}</div>`).join('')}</div>
        </div>`;

    // 3. 准备饼图数据
    const pieData = [
        { value: highCount, name: 'High Influenced' },
        { value: midCount, name: 'Medium Influenced' },
        { value: lowCount, name: 'Low Influenced' },
        { value: seedCount, name: 'Seed Nodes' },
        { value: uninfluencedCount, name: 'Unaffected' }
    ];
    const pieColors = [hot, mid, cold, seedNode, inactive];

    // 4. 渲染最终HTML
    resultsDiv.innerHTML = `
        ${keyMetricsHtml}
        ${seedNodesHtml}
        <h4 class="chart-title">📊 Influence Distribution</h4>
        <div id="interactive-max-pie-chart" class="chart-container"></div>
        <h4 class="chart-title">📉 Propagation Timeline</h4>
        <p class="info-text" style="text-align: center;">Timeline chart is not available in interactive mode.</p>
        <div id="log-messages"></div>
    `;

    // 5. 初始化饼图
    initPieChart('interactive-max-pie-chart', 'Live Influence Distribution', pieData, pieColors);
}

/**
 * Renders the results for interactive 'minimization' mode.
 */
export function renderInteractiveMinimizationResults() {
    const influenceCount = Array.from(state.probabilityMap.values()).filter(v => v.state === 'active').length;
    let blockingNodesHtml = `<p class="info-text">None. Click nodes to block.</p>`;
    if (state.interactiveBlockingNodes.size > 0) {
        blockingNodesHtml = `<div class="node-chip-list">${[...state.interactiveBlockingNodes].map(id => `<div class="node-chip" onclick="window.highlightNode('${id}')">${id}</div>`).join('')}</div>`;
    }
    resultsDiv.innerHTML = `
        <div class="result-section"><h4>🛡️ Interactive Blocking Nodes (${state.interactiveBlockingNodes.size})</h4>${blockingNodesHtml}</div>
        <div class="result-section"><h4>📉 Real-time Influence</h4><div class="metric"><span class="label">Influenced Nodes</span><span class="value">${influenceCount.toLocaleString()}</span></div></div>
        <div id="log-messages" class="info-text">${state.interactiveBlockingNodes.size > 0 ? '' : 'Baseline influence shown.'}</div>`;
}

export function renderFullInteractiveMinimizationResults() {
    const totalNodes = state.Graph.graphData().nodes.length;

    // 1. 计算当前影响力及饼图所需数据
    const stillInfluenced = Array.from(state.probabilityMap.values()).filter(v => v.state === 'active').length;
    const savedCount = Math.max(0, state.baselineInfluenceCount - stillInfluenced);
    const unaffected = totalNodes - state.baselineInfluenceCount;
    const savedRatio = state.baselineInfluenceCount > 0 ? (savedCount / state.baselineInfluenceCount * 100).toFixed(1) : 0;

    // 2. 准备关键指标和节点列表的HTML
    const keyMetricsHtml = `
        <h4 class="result-section-title">⭐ Key Metrics</h4>
        <div class="key-metrics-container">
            <div class="key-metric-item">
                <span class="metric-icon">🛡️</span>
                <div class="metric-text">
                    <span class="metric-label">Nodes Saved from Influence</span>
                    <span class="metric-value">${savedCount.toLocaleString()} (${savedRatio}%)</span>
                </div>
            </div>
        </div>
    `;

    let blockingNodesHtml = `<div class="result-section"><h4>🛡️ Interactive Blocking Nodes (${state.interactiveBlockingNodes.size})</h4><div class="node-chip-list">${[...state.interactiveBlockingNodes].map(id => `<div class="node-chip" onclick="window.highlightNode('${id}')">${id}</div>`).join('') || '<p class="info-text" style="width:100%; text-align:center;">None selected. Click nodes to block.</p>'}</div></div>`;

    // 3. 准备饼图数据
    const pieData = [
        { value: savedCount, name: 'Saved Nodes' },
        { value: stillInfluenced, name: 'Still Influenced' },
        { value: unaffected, name: 'Uninfluenced' }
    ];
    const pieColors = [state.currentPalette.recovered, state.currentPalette.hot, state.currentPalette.inactive];

    // 4. 渲染最终HTML
    resultsDiv.innerHTML = `
        ${keyMetricsHtml}
        ${blockingNodesHtml}
        <h4 class="chart-title">📊 Blocking Effectiveness</h4>
        <div id="interactive-min-pie-chart" class="chart-container"></div>
        <h4 class="chart-title">📉 Saved Nodes Timeline</h4>
        <p class="info-text" style="text-align: center;">Timeline chart is not available in interactive mode.</p>
        <div id="log-messages"></div>
    `;

    // 5. 初始化饼图
    initPieChart('interactive-min-pie-chart', 'Live Blocking Effectiveness', pieData, pieColors);
}

/**
 * Appends a log message to the log area in the results panel.
 * @param {string} message - The message to log.
 * @param {'info' | 'error' | 'success'} type - The type of message.
 */
export function appendLog(message, type = 'info') {
    const logArea = document.getElementById('log-messages');
    if (logArea) logArea.innerHTML = `<p class="log-${type}">${message}</p>`;
}

/**
 * Handles the logic for the critical path analysis button.
 * @param {string} resultId - The ID of the analysis result.
 * @param {HTMLElement} button - The button element that was clicked.
 */
async function handleCriticalPathAnalysis(resultId, button) {
    button.disabled = true;
    button.textContent = '⏳ Analyzing...';
    state.criticalPathLinks.clear();
    appendLog('Identifying critical propagation chain...');

    const pathContainer = document.getElementById('critical-path-results');
    if (pathContainer) pathContainer.innerHTML = ''; // 清空旧路径

    try {
        const data = await Api.runCriticalPathAnalysis(resultId);
        appendLog(data.message);

        if (data.critical_paths?.length > 0) {
            const path = data.critical_paths[0];
            const pathNodes = path.nodes;

            for (let i = 0; i < pathNodes.length - 1; i++) {
                state.criticalPathLinks.add(`${Math.min(pathNodes[i], pathNodes[i + 1])}-${Math.max(pathNodes[i], pathNodes[i + 1])}`);
            }

            // [!code-start]
            // 【核心修改】渲染关键路径节点列表到结果面板
            if (pathContainer) {
                const pathHtml = `
                    <h4 class="result-section-title">🔗 Critical Chain Path</h4>
                    <div class="path-node-list">
                        ${pathNodes.map(nodeId =>
                    `<div class="path-node-item" onclick="window.highlightNode('${nodeId}')">${nodeId}</div>`
                ).join('<span class="path-separator">→</span>')}
                    </div>
                `;
                pathContainer.innerHTML = pathHtml;
            }
            // [!code-end]
        }
    } catch (error) {
        appendLog(`Analysis failed: ${error.message}`, 'error');
    } finally {
        button.disabled = false;
        button.textContent = '🔗 Find Critical Chain';
        Graph.updateLinkVisuals();
    }
}

// js/ui.js

export function createTimelineControl(container, totalSteps) {
    if (!container || totalSteps <= 1) {
        container.innerHTML = ''; // Don't render for trivial animations
        return;
    }

    let stepsHtml = '';
    for (let i = 0; i < totalSteps; i++) {
        const isEndpoint = (i === 0 || i === totalSteps - 1);
        const endpointClass = isEndpoint ? 'timeline-step-endpoint' : '';
        let label = '';
        if (i === 0) label = '<div class="timeline-label">Start</div>';
        if (i === totalSteps - 1) label = '<div class="timeline-label">End</div>';

        stepsHtml += `
            <li id="timeline-step-${i}" class="timeline-step ${endpointClass}">
                <div class="timeline-marker"></div>
                ${label}
            </li>
        `;
    }

    // [!code-start]
    // 【【【核心修复】】】为两个按钮添加 type="button"
    container.innerHTML = `
        <div class="timeline-container">
            <div style="position: relative; margin: 0 10px;">
                <div id="timeline-progress" class="timeline-progress"></div>
                <ol class="timeline-track" style="margin: 0;">
                    ${stepsHtml}
                </ol>
            </div>
             <div class="control-row" style="margin-top: 20px; justify-content: center; gap: 10px; display: flex;">
                 <button type="button" class="action-button action-button-secondary" id="play-pause-btn" style="width: 100px;">Play</button>
                <button type="button" class="action-button action-button-secondary" id="step-forward-btn" style="width: 100px;">Step</button>
            </div>
        </div>
    `;
    // [!code-end]
}
// 替换现有的 initPieChart 函数
function initPieChart(elementId, title, data, colors) {
    const chartDom = document.getElementById(elementId);
    if (!chartDom) return;

    echarts.dispose(chartDom);

    const rootStyles = getComputedStyle(document.documentElement);
    const bodyFontSize = parseInt(rootStyles.getPropertyValue('--font-size-body').trim(), 10) || 14;
    const smallFontSize = parseInt(rootStyles.getPropertyValue('--font-size-small').trim(), 10) || 13;
    const tertiaryFontColor = rootStyles.getPropertyValue('--font-color-tertiary').trim() || '#888';
    const secondaryFontColor = rootStyles.getPropertyValue('--font-color-secondary').trim() || '#555';

    const myChart = echarts.init(chartDom, state.currentPalette === THEME_PALETTES.dark ? 'dark' : null);

    // 【【【核心修改：动态布局】】】
    // 根据图例项的数量来决定布局参数
    let seriesCenterY, gridBottom, gridTop,radius;
    const legendItemCount = data.length;

    if (legendItemCount <= 3) { 
        // 场景：图例很少（如最小化模式），可以更紧凑
        seriesCenterY = '45%'; // 将饼图垂直居中，因为它不需要为下方的图例预留太多空间
        gridBottom = 10;       // 大幅减少底边距
        gridTop = 5;          // 减少顶边距
        radius = '65%'; 
    } else if (legendItemCount == 4) { 
        // 场景：图例很少（如最小化模式），可以更紧凑
        seriesCenterY = '40%'; // 将饼图垂直居中，因为它不需要为下方的图例预留太多空间
        gridBottom = 10;       // 大幅减少底边距
        gridTop = 5;          // 减少顶边距
        radius = '55%'; 
    }else { 
        // 场景：图例很多（如最大化模式），可能换行，需要更多空间
        seriesCenterY = '33%'; // 将饼图向上移动，为下方可能换行的图例留出空间
        gridBottom = 85;       // 增加底边距以容纳两行图例
        gridTop = 30;          // 保持正常的顶边距
        radius = '45%'; 
    }

    const option = {
        legend: {
            orient: 'horizontal',
            bottom: 0,
            left: 'center',
            itemGap: 15,
            textStyle: {
                fontSize: smallFontSize,
                color: state.currentPalette === THEME_PALETTES.dark ? '#fff' : '#444'
            }
        },
        series: [{
            name: title,
            type: 'pie',
            radius: radius,
            // 使用我们动态计算出的垂直中心点
            center: ['50%', seriesCenterY], 
            data: data,
            label: {
                show: true,
                position: 'outside',
                formatter: '{name|{b}}\n{val|{c} ({d}%)}',
                color: state.currentPalette === THEME_PALETTES.dark ? '#fff' : secondaryFontColor,
                rich: {
                    name: { fontSize: smallFontSize-2, lineHeight: 18, color: state.currentPalette === THEME_PALETTES.dark ? '#eee' : secondaryFontColor },
                    val: { fontSize: smallFontSize-2, lineHeight: 16, color: tertiaryFontColor }
                }
            },
            labelLine: { length: 15, length2: 12, show: true },
            emphasis: { itemStyle: { shadowBlur: 10, shadowOffsetX: 0, shadowColor: 'rgba(0, 0, 0, 0.5)' } },
            avoidLabelOverlap: true
        }],
        grid: {
            // 使用我们动态计算出的边距
            top: gridTop,
            left: '5%',
            right: '5%',
            bottom: gridBottom,
            containLabel: true
        },
        color: colors
    };
    myChart.setOption(option);
}

/**
 * Initializes a line chart for propagation or blocking timeline.
 * @param {string} elementId - The ID of the container element.
 * @param {string} title - The chart title.
 * @param {Array<object>} stepsData - The raw simulation_steps array.
 * @param {string} mode - 'maximization' or 'minimization'.
 */
// 替换现有的 initLineChart 函数
function initLineChart(elementId, title, stepsData, mode) {
    const chartDom = document.getElementById(elementId);
    if (!chartDom) return;

    let chartData, tooltipFormatter, yAxisName, seriesColor;
    const initialSeedSize = state.specialNodeIds.size;

    if (mode === 'maximization') {
        chartData = stepsData.map((step, index) => {
            // 1. 获取当前步骤快照中的激活节点数
            const activeNodesInSnapshot = (step.node_states || []).filter(n => n.state === 'active').length;
            // 2. 总数 = 快照中的数量 + 初始种子数
            const totalInfluenced = activeNodesInSnapshot + initialSeedSize;
            return [index, totalInfluenced];
        });
        // [!code-end]
        tooltipFormatter = 'Step {b}:<br/>{c} Influenced Nodes';
        yAxisName = 'Total Influenced Nodes';
        
        seriesColor = state.currentPalette.hot;
    } else { // Minimization
        let savedNodesCumulative = 0;
        chartData = stepsData.map((step, index) => {
            savedNodesCumulative += (step.newly_recovered_nodes || []).length;
            return [index, savedNodesCumulative];
        });
        tooltipFormatter = 'Step {b}:<br/>{c} Nodes Saved';
        yAxisName = 'Total Nodes Saved';
        seriesColor = state.currentPalette.recovered;
    }

    echarts.dispose(chartDom);

    const myChart = echarts.init(chartDom, state.currentPalette === THEME_PALETTES.dark ? 'dark' : null);
    const rootStyles = getComputedStyle(document.documentElement);
    const bodyFontSize = parseInt(rootStyles.getPropertyValue('--font-size-body').trim(), 10) || 14;
    const smallFontSize = parseInt(rootStyles.getPropertyValue('--font-size-small').trim(), 10) || 13;
    const option = {
        title: {
            // 移除这里的 title，因为我们已经在HTML中有了 .chart-title
        },
        tooltip: {
            trigger: 'axis',
            formatter: function (params) {
                // params 是一个包含当前X轴上所有系列数据点的数组
                if (params && params.length > 0) {
                    const step = params[0].axisValue;      // 获取X轴的数值，例如：1
                    const value = params[0].value[1];      // 获取Y轴的数值，例如：437

                    // 根据模式手动构建正确的提示字符串
                    if (mode === 'maximization') {
                        return `Step ${step}:<br/>${value.toLocaleString()} Influenced Nodes`;
                    } else { // Minimization
                        return `Step ${step}:<br/>${value.toLocaleString()} Nodes Saved`;
                    }
                }
                return ''; // 如果没有数据则返回空
            }
        },
        xAxis: { type: 'value', name: 'Simulation Step', nameLocation: 'middle', nameGap: 25, nameTextStyle: { fontSize: bodyFontSize }, axisLabel: { fontSize: smallFontSize } },
        yAxis: { type: 'value', name: yAxisName, nameTextStyle: { fontSize: bodyFontSize }, axisLabel: { fontSize: smallFontSize } },
        series: [{ data: chartData, type: 'line', smooth: true, areaStyle: {}, symbolSize: 8, }],
        grid: {
            top: 50, // 【新增】设置绘图区距离容器顶部40像素
            left: '20%', 
            right: '8%',
            bottom: '20',
            containLabel: true 
        },
        color: [seriesColor]
    };
    console.log('Final Chart Data for ECharts:', chartData);
    myChart.setOption(option);
}

// 文件: js/ui.js

export function updateTimelineUI(currentStep, totalSteps) {
    const progress = document.getElementById('timeline-progress');
    if (!progress || totalSteps <= 1) return;

    // [!code-start]
    // 【核心修复】使用 transform: scaleX 来控制进度条，确保与圆点精确对齐
    const ratio = currentStep / (totalSteps - 1);
    progress.style.transform = `scaleX(${ratio})`;
    // [!code-end]

    // 更新圆点标记的状态
    for (let i = 0; i < totalSteps; i++) {
        const step = document.getElementById(`timeline-step-${i}`);
        if (step) {
            if (i <= currentStep) {
                step.classList.add('completed');
            } else {
                step.classList.remove('completed');
            }
        }
    }
}
/**
 * [!code-start]
 * 【新增】处理手动步进动画的逻辑
 */
function handleStepForward() {
    if (!state.animationSteps || state.animationSteps.length === 0) return;

    // 1. 如果动画正在自动播放，先暂停它
    if (state.isPlaying) {
        Graph.pauseAnimation(); // 调用从 graph.js 导出的函数
        UI.setPlayButtonState(false); // 更新 "Play" 按钮的文本
    }

    // 2. 计算下一步
    let nextStep = state.currentStep + 1;
    const totalSteps = state.animationSteps.length;

    // 3. 确保步数不会超过最大值
    if (nextStep >= totalSteps) {
        nextStep = totalSteps - 1;
    }

    // 4. 更新状态并调用 graph.js 中的函数来更新UI
    if (nextStep !== state.currentStep) {
        state.currentStep = nextStep;
        // 【【【核心修复】】】确保这里调用的是 updateToStep
        Graph.updateToStep(state.currentStep); // 调用从 graph.js 导出的函数
    }
}
// [!code-end]

/**
 * Manages the play/pause button state.
 * @param {boolean} isPlaying - The current playing state.
 */
export function setPlayButtonState(isPlaying) {
    const playPauseBtn = document.getElementById('play-pause-btn');
    if (playPauseBtn) {
        playPauseBtn.textContent = isPlaying ? '⏸️ Pause' : '▶️ Play';
    }
}
// [!code-end]