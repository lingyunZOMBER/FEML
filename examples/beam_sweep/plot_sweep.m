%% ========================================================================
%  作业 2：Timoshenko 梁单元 —— 批量扫描结果绘图
%
%  【数据来源】
%      C++ 扫描驱动输出 beam_sweep.csv。在仓库根目录运行：
%          out\build\x64-Debug\bin\beam_sweep.exe examples\beam_sweep\beam_sweep.csv
%
%  【本脚本做什么】
%      只做"读数据 + 画图"。所有有限元计算都在 C++ 里完成，
%      这里不复算任何力学量 —— 避免出现两套实现结果不一致时无从判断。
%
%  【CSV 列】
%      elementType         BEAM2 / BEAM3
%      elementCount        单元数
%      scheme              FULL / REDUCED / SELECTIVE
%      loadType            LOAD（端部集中力）/ MOMENT（端部弯矩）
%      hOverL              截面高 / 梁长
%      tipW, tipTheta      数值解
%      w_timoshenko        Timoshenko 解析解（含剪切）
%      w_eulerBernoulli    材料力学解（Euler-Bernoulli，不含剪切）
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

% ---- 派生量 ----
% 相对材料力学解的比值：这是作业要求讨论的"差异"
T.ratioW = T.tipW     ./ T.w_eulerBernoulli;
T.ratioT = T.tipTheta ./ T.theta_eulerBernoulli;

% 节点数 / 自由度数（用于在"同等计算量"下比较两种单元）
T.nodeCount = T.elementCount + 1;
is3 = (T.elementType == "BEAM3");
T.nodeCount(is3) = 2 * T.elementCount(is3) + 1;
T.dof = 2 * T.nodeCount;

fprintf('读入 %d 行数据\n', height(T));
fprintf('  单元类型: %s\n', strjoin(unique(T.elementType)', ', '));
fprintf('  积分方案: %s\n', strjoin(unique(T.scheme)', ', '));
fprintf('  载荷类型: %s\n\n', strjoin(unique(T.loadType)', ', '));

colors  = lines(6);
markers = {'o', 's', '^', 'd', 'v', '>'};

%% ========================================================================
%  图 1：细长比扫描 —— 作业的核心问题
%
%  1 个单元、端部集中力，横轴 h/L（对数），纵轴 数值解 / 材料力学解。
%
%  预期现象：
%    BEAM2 + FULL      → 趋于 0        （剪切锁死）
%    BEAM2 + REDUCED   → 趋于 3/4      （不锁死，但有系统偏刚）
%    BEAM3 + FULL      → 趋于 3/4      （二次插值保留了纯弯模式）
%    BEAM3 + REDUCED   → 恒等于 1      （1 个单元就精确）
% =========================================================================

S = T(T.elementCount == 1 & T.loadType == "LOAD", :);

fig1 = figure('Name', 'Fig 1 - Locking vs slenderness', ...
              'Color', 'w', 'Position', [100 100 900 600]);
hold on;

types   = ["BEAM2", "BEAM3"];
schemes = ["FULL",  "REDUCED"];
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

set(gca, 'XScale', 'log');
xlabel('h / L', 'FontSize', 12);
ylabel('w_{num} / w_{Euler-Bernoulli}', 'FontSize', 12);
title('1 个单元，端部集中力：细长比趋于 0 时的挠度比', 'FontSize', 13);
legend('Location', 'southwest', 'FontSize', 10);
grid on; box on;
ylim([0 1.2]);
yline(1.0, 'k:', 'HandleVisibility', 'off');
yline(0.75, 'k--', 'HandleVisibility', 'off');
text(1.2e-3, 0.78, '3/4', 'FontSize', 10);
drawnow;

%% ========================================================================
%  图 2：网格收敛 —— 固定细长比，看加密网格的效果
% =========================================================================

hl = 0.05;
S = T(abs(T.hOverL - hl) < 1e-12 & T.loadType == "LOAD", :);

fig2 = figure('Name', 'Fig 2 - Mesh convergence', ...
              'Color', 'w', 'Position', [150 150 900 600]);
hold on;

k = 0;
for it = 1:numel(types)
    for is = 1:numel(schemes)
        k = k + 1;
        sub = S(S.elementType == types(it) & S.scheme == schemes(is), :);
        if isempty(sub), continue; end
        sub = sortrows(sub, 'dof');

        semilogy(sub.dof, abs(1 - sub.ratioW), '-', ...
                 'Color', colors(k,:), 'LineWidth', 1.8, ...
                 'Marker', markers{is}, 'MarkerSize', 7, ...
                 'MarkerFaceColor', 'w', ...
                 'DisplayName', sprintf('%s + %s', types(it), schemes(is)));
    end
end

% 材料力学解与 Timoshenko 解之间的固有差异（剪切效应）
shearGap = abs(1 - S.w_timoshenko(1) / S.w_eulerBernoulli(1));
yline(shearGap, 'k--', 'LineWidth', 1.2, ...
      'DisplayName', '剪切效应（材料力学解的固有误差）');

set(gca, 'XScale', 'log', 'YScale', 'log');
xlabel('自由度数', 'FontSize', 12);
ylabel('|1 - w_{num} / w_{Euler-Bernoulli}|', 'FontSize', 12);
title(sprintf('网格收敛（h/L = %.2f，端部集中力）', hl), 'FontSize', 13);
legend('Location', 'southwest', 'FontSize', 10);
grid on; box on;
drawnow;

%% ========================================================================
%  图 3：纯弯 vs 端部集中力 —— 两种载荷下的误差来源不同
%
%  纯弯时剪切应变为零，所以：
%    - 减缩积分在【任意】单元数和细长比下都精确
%    - 误差完全来自弯曲项，与剪切锁死无关
% =========================================================================

hl = 0.1;
S = T(abs(T.hOverL - hl) < 1e-12, :);

fig3 = figure('Name', 'Fig 3 - Moment vs Load', ...
              'Color', 'w', 'Position', [200 200 1000 600]);

loadTypes = ["MOMENT", "LOAD"];
titles = {'端部弯矩（纯弯）', '端部集中力'};

for il = 1:2
    subplot(1, 2, il);
    hold on;
    k = 0;

    S2 = S(S.loadType == loadTypes(il), :);

    for it = 1:numel(types)
        for is = 1:numel(schemes)
            k = k + 1;
            sub = S2(S2.elementType == types(it) & S2.scheme == schemes(is), :);
            if isempty(sub), continue; end
            sub = sortrows(sub, 'dof');

            semilogy(sub.dof, abs(1 - sub.ratioW), '-', ...
                     'Color', colors(k,:), 'LineWidth', 1.6, ...
                     'Marker', markers{is}, 'MarkerSize', 6, ...
                     'MarkerFaceColor', 'w', ...
                     'DisplayName', sprintf('%s + %s', types(it), schemes(is)));
        end
    end

    set(gca, 'XScale', 'log', 'YScale', 'log');
    xlabel('自由度数', 'FontSize', 11);
    ylabel('|1 - w_{num} / w_{Euler-Bernoulli}|', 'FontSize', 11);
    title(sprintf('%s（h/L = %.2f）', titles{il}, hl), 'FontSize', 12);
    grid on; box on;
    if il == 1
        legend('Location', 'southwest', 'FontSize', 9);
    end
end

sgtitle('纯弯与集中力：误差来源的差异', 'FontSize', 13);
drawnow;

%% ========================================================================
%  文字汇总：把关键数字打印出来，方便直接写进报告
% =========================================================================

fprintf('%s\n', repmat('=', 1, 80));
fprintf('表 1  1 个单元、端部集中力：w_num / w_Euler-Bernoulli\n');
fprintf('%s\n', repmat('-', 1, 80));

fprintf('%-8s', 'h/L');
for it = 1:numel(types)
    for is = 1:numel(schemes)
        fprintf('%-15s', sprintf('%s+%s', types(it), schemes(is)));
    end
end
fprintf('\n');

S = sortrows(T(T.elementCount == 1 & T.loadType == "LOAD", :), 'hOverL');
hlList = unique(S.hOverL, 'stable');

for h = hlList'
    fprintf('%-8.3f', h);
    for it = 1:numel(types)
        for is = 1:numel(schemes)
            v = S.ratioW(S.hOverL == h & S.elementType == types(it) ...
                         & S.scheme == schemes(is));
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

%% ========================================================================
%  保存图片
% =========================================================================

outDir = fileparts(mfilename('fullpath'));
exportgraphics(fig1, fullfile(outDir, 'fig1_locking.png'),        'Resolution', 200);
exportgraphics(fig2, fullfile(outDir, 'fig2_convergence.png'),    'Resolution', 200);
exportgraphics(fig3, fullfile(outDir, 'fig3_moment_vs_load.png'), 'Resolution', 200);

fprintf('图已生成并保存到:\n  %s\n', outDir);
fprintf('  1) fig1_locking.png           细长比扫描（锁死现象）\n');
fprintf('  2) fig2_convergence.png       网格收敛\n');
fprintf('  3) fig3_moment_vs_load.png    纯弯 vs 集中力\n');
