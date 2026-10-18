%% ========================================================================
%  作业 2：Timoshenko 梁单元 —— 批量扫描结果绘图
%
%  【数据来源】
%      C++ 扫描驱动输出 beam_sweep.csv。在仓库根目录运行：
%          out\build\x64-Debug\bin\beam_sweep.exe examples\beam_sweep\beam_sweep.csv
%
%  【本脚本做什么】
%      只做"读数据 + 画图"。所有有限元计算都在 C++ 里完成。
%
%  【CSV 列】
%      elementType         BEAM2 / BEAM3
%      elementCount        单元数
%      scheme              FULL / REDUCED / SELECTIVE
%      loadType            LOAD（端部集中力）/ MOMENT（端部弯矩）
%      hOverL              截面高 / 梁长
%      tipW, tipTheta      数值解（端部挠度、转角）
%
%  【参考解】本脚本自行计算材料力学解（Euler-Bernoulli），用于归一化
%
%  【输出】
%      fig1_locking.png           1 个单元：挠度比 vs 细长比（总览）
%      fig2_load_w.png            端部集中力：挠度比 vs 细长比（按单元数）
%      fig3_load_theta.png        端部集中力：转角比 vs 细长比（按单元数）
%      fig4_moment_w.png          端部弯矩：挠度比 vs 细长比（按单元数）
%      fig5_moment_theta.png      端部弯矩：转角比 vs 细长比（按单元数）
% =========================================================================

clear; clc; close all;

csvFile = fullfile(fileparts(mfilename('fullpath')), 'beam_sweep.csv');
if ~isfile(csvFile)
    error(['找不到 %s\n' ...
           '请先在仓库根目录运行:\n' ...
           '  out\\build\\x64-Debug\\bin\\beam_sweep.exe examples\\beam_sweep\\beam_sweep.csv'], ...
           csvFile);
end

T = readtable(csvFile, 'TextType', 'string');

% ---- 材料力学解（Euler-Bernoulli）----
%
% 悬臂梁端部集中力 P：w = PL³/(3EI)，θ = PL²/(2EI)
% 悬臂梁端部弯矩 M：  w = ML²/(2EI)，θ = ML/(EI)
E  = 200.0e9;   % 弹性模量 [Pa]
nu = 0.3;       % 泊松比
L  = 1.0;       % 梁长 [m]
b  = 0.1;       % 截面宽 [m]
P  = 1000.0;    % 端部集中力 [N]
M  = 1000.0;    % 端部弯矩 [N·m]

h  = T.hOverL * L;
I  = b * h.^3 / 12.0;
EI = E * I;

isLoad = (T.loadType == "LOAD");

T.wEB = M * L^2 ./ (2*EI);
T.tEB = M * L     ./ EI;

T.wEB(isLoad) = P * L^3 ./ (3*EI(isLoad));
T.tEB(isLoad) = P * L^2 ./ (2*EI(isLoad));

% 相对材料力学解的比值
T.ratioW = T.tipW     ./ T.wEB;
T.ratioT = T.tipTheta ./ T.tEB;

% 节点数 / 自由度数
T.nodeCount = T.elementCount + 1;
is3 = (T.elementType == "BEAM3");
T.nodeCount(is3) = 2 * T.elementCount(is3) + 1;
T.dof = 2 * T.nodeCount;

outDir = fileparts(mfilename('fullpath'));

types    = ["BEAM2", "BEAM3"];
schemes  = ["FULL",  "REDUCED"];
elementCounts = unique(T.elementCount);

fprintf('读入 %d 行数据\n', height(T));
fprintf('  单元类型: %s\n', strjoin(unique(T.elementType)', ', '));
fprintf('  积分方案: %s\n', strjoin(unique(T.scheme)', ', '));
fprintf('  载荷类型: %s\n', strjoin(unique(T.loadType)', ', '));
fprintf('  单元数  : %s\n\n', strjoin(string(elementCounts)', ', '));

colors  = lines(6);
markers = {'o', 's', '^', 'd', 'v', '>'};

%% ========================================================================
%  图 1：1 个单元时的总览
%
%  横轴 h/L（对数），纵轴 数值解 / 材料力学解。
%  四条曲线对应两种单元 × 两种积分方案。
% =========================================================================

S = T(T.elementCount == 1 & T.loadType == "LOAD", :);

fig1 = figure('Name', 'Fig 1 - Locking vs slenderness', ...
              'Color', 'w', 'Position', [100 100 950 620]);
hold on;

k = 0;
for it = 1:numel(types)
    for is = 1:numel(schemes)
        k = k + 1;
        sub = S(S.elementType == types(it) & S.scheme == schemes(is), :);
        if isempty(sub), continue; end
        sub = sortrows(sub, 'hOverL');

        plot(sub.hOverL, sub.ratioW, '-', ...
             'Color', colors(k,:), 'LineWidth', 1.8, ...
             'Marker', markers{is}, 'MarkerSize', 7, ...
             'MarkerFaceColor', 'w', ...
             'DisplayName', sprintf('%s + %s', types(it), schemes(is)));
    end
end

set(gca, 'XScale', 'log', 'XDir', 'reverse');
xlabel('h / L', 'FontSize', 12);
ylabel('w_{num} / w_{Euler-Bernoulli}', 'FontSize', 12);
title('1 个单元、端部集中力：细长比趋于 0 时的挠度比', 'FontSize', 13);
legend('Location', 'northoutside', 'Orientation', 'horizontal', 'FontSize', 9);
grid on; box on;
ylim([0 1.3]);
yline(1.0, 'k:', 'HandleVisibility', 'off');
yline(0.75, 'k--', 'HandleVisibility', 'off');
text(1.2e-3, 0.78, '3/4', 'FontSize', 10);
drawnow;
exportgraphics(fig1, fullfile(outDir, 'fig1_locking.png'), 'Resolution', 200);

%% ========================================================================
%  图 2~5：不同单元数下的细长比扫描
%
%  每幅图 2×2 面板：
%      行 = 单元类型（BEAM2 / BEAM3）
%      列 = 积分方案（FULL / REDUCED）
%  每个面板内，按单元数画出多条曲线（颜色由冷到暖 = 单元数由少到多）。
%
%  覆盖的物理量：
%      端部集中力的挠度 / 转角
%      端部弯矩的挠度 / 转角
% =========================================================================

cases = {
    'LOAD',   'ratioW', 'fig2_load_w.png',      '端部集中力：挠度比', 'w_{num} / w_{Euler-Bernoulli}';
    'LOAD',   'ratioT', 'fig3_load_theta.png',  '端部集中力：转角比', '\theta_{num} / \theta_{Euler-Bernoulli}';
    'MOMENT', 'ratioW', 'fig4_moment_w.png',    '端部弯矩：挠度比',   'w_{num} / w_{Euler-Bernoulli}';
    'MOMENT', 'ratioT', 'fig5_moment_theta.png','端部弯矩：转角比',   '\theta_{num} / \theta_{Euler-Bernoulli}'
};

cmap = turbo(numel(elementCounts));

for ic = 1:size(cases, 1)
    loadType = cases{ic, 1};
    qty      = cases{ic, 2};
    fname    = cases{ic, 3};
    ftitle   = cases{ic, 4};
    ylab     = cases{ic, 5};

    f = figure('Name', ftitle, 'Color', 'w', 'Position', [120 120 1100 820]);
    tl = tiledlayout(f, 2, 2, 'TileSpacing', 'compact', 'Padding', 'compact');

    lastAx = [];
    for it = 1:numel(types)
        for is = 1:numel(schemes)
            lastAx = nexttile(tl);
            hold on;

            S2 = T(T.loadType == loadType & T.elementType == types(it) ...
                   & T.scheme == schemes(is), :);

            for ie = 1:numel(elementCounts)
                sub = S2(S2.elementCount == elementCounts(ie), :);
                if isempty(sub), continue; end
                sub = sortrows(sub, 'hOverL');

                plot(sub.hOverL, sub.(qty), '-', ...
                     'Color', cmap(ie,:), 'LineWidth', 1.4, ...
                     'Marker', 'o', 'MarkerSize', 3.5, ...
                     'MarkerFaceColor', cmap(ie,:), ...
                     'DisplayName', sprintf('%d 单元', elementCounts(ie)));
            end

            set(gca, 'XScale', 'log', 'XDir', 'reverse');
            xlabel('h / L', 'FontSize', 10);
            ylabel(ylab, 'FontSize', 10);
            title(sprintf('%s + %s', types(it), schemes(is)), 'FontSize', 11);
            grid on; box on;
            ylim([0 1.35]);
        end
    end

    title(tl, ftitle, 'FontSize', 13);
    lgd = legend(lastAx, 'Orientation', 'horizontal', 'NumColumns', 4, 'FontSize', 8);
    lgd.Layout.Tile = 'south';

    exportgraphics(f, fullfile(outDir, fname), 'Resolution', 200);
end

%% ========================================================================
%  文字汇总：1 个单元、端部集中力
% =========================================================================

fprintf('%s\n', repmat('=', 1, 80));
fprintf('表 1  1 个单元、端部集中力：w_num / w_Euler-Bernoulli\n');
fprintf('%s\n', repmat('-', 1, 80));

allSchemes = ["FULL", "REDUCED", "SELECTIVE"];
fprintf('%-8s', 'h/L');
for it = 1:numel(types)
    for is = 1:numel(allSchemes)
        fprintf('%-15s', sprintf('%s+%s', types(it), allSchemes(is)));
    end
end
fprintf('\n');

S = sortrows(T(T.elementCount == 1 & T.loadType == "LOAD", :), 'hOverL');
hlList = unique(S.hOverL, 'stable');

for h = hlList'
    fprintf('%-8.3f', h);
    for it = 1:numel(types)
        for is = 1:numel(allSchemes)
            v = S.ratioW(S.hOverL == h & S.elementType == types(it) ...
                         & S.scheme == allSchemes(is));
            if isempty(v)
                fprintf('%-15s', '-');
            else
                fprintf('%-15.4f', v(1));
            end
        end
    end
    fprintf('\n');
end
fprintf('%s\n\n', repmat('=', 1, 80));

fprintf('图已生成并保存到:\n  %s\n', outDir);
fprintf('  1) fig1_locking.png       1 个单元：挠度比 vs 细长比（总览）\n');
fprintf('  2) fig2_load_w.png        端部集中力：挠度比（按单元数）\n');
fprintf('  3) fig3_load_theta.png    端部集中力：转角比（按单元数）\n');
fprintf('  4) fig4_moment_w.png      端部弯矩：挠度比（按单元数）\n');
fprintf('  5) fig5_moment_theta.png  端部弯矩：转角比（按单元数）\n');
