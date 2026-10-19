%% ========================================================================
%  作业 3：Mindlin 板单元锁死试验（对应课本图 10.12）
%
%  【数据来源】
%      plate_sweep.exe 输出 plate_sweep.csv
%
%  【算例】
%      四边固支方板，边长 L，厚 t，均布载荷 q
%      用 8×8 网格离散（每边 8 个单元）
%      纵轴：w_c·D/(q·L⁴)，横轴：L/t
%      精确薄板解：0.00126
% =========================================================================

clear; clc; close all;

csvFile = fullfile(fileparts(mfilename('fullpath')), 'plate_sweep.csv');
if ~isfile(csvFile)
    error('找不到 %s，请先运行 plate_sweep.exe', csvFile);
end

T = readtable(csvFile, 'TextType', 'string');
outDir = fileparts(mfilename('fullpath'));

types   = ["PLATE9", "PLATE8"];
schemes = ["FULL", "REDUCED", "SELECTIVE"];
labels  = {'F', 'R', 'S'};
styles  = {'--', '-', '-.'};
colors  = [0.85 0.33 0.10; 0.00 0.45 0.74; 0.47 0.67 0.19];

fprintf('读入 %d 行数据\n\n', height(T));

%% ---- 图：1 行 2 列（两种单元），每格三条曲线 ----

fig = figure('Name', 'Plate locking test', 'Color', 'w', ...
             'Position', [100 100 1150 480]);
tl = tiledlayout(fig, 1, 2, 'TileSpacing', 'compact', 'Padding', 'compact');

for it = 1:numel(types)
    ax = nexttile(tl);
    hold on;

    for is = 1:numel(schemes)
        S = T(T.elementType == types(it) & T.scheme == schemes(is), :);
        if isempty(S), continue; end
        S = sortrows(S, 'Lt');

        semilogx(S.Lt, S.wcD_qL4, styles{is}, ...
                 'Color', colors(is,:), 'LineWidth', 1.8, ...
                 'Marker', '.', 'MarkerSize', 8, ...
                 'DisplayName', labels{is});
    end

    yline(0.00126, 'k--', 'LineWidth', 1.0, 'HandleVisibility', 'off');

    set(gca, 'XScale', 'log');
    xlim([10 1000]);
    ylim([0.0009 0.0018]);
    xticks([10 20 50 100 200 500 1000]);
    xlabel('L/t', 'FontSize', 12);
    ylabel('w_c D / (q L^4)', 'FontSize', 12);
    title(types(it), 'FontSize', 13);
    grid on; box on;

    legend('Location', 'southwest', 'FontSize', 10);
end

title(tl, '8×8 网格的锁死试验（虚线为精确薄板解 0.00126）', 'FontSize', 13);

exportgraphics(fig, fullfile(outDir, 'fig1_plate.png'), 'Resolution', 200);

%% ---- 文字汇总 ----

fprintf('%s\n', repmat('=', 1, 92));
fprintf('w_c D / (q L^4)，精确薄板解 = 0.00126\n');
fprintf('%s\n', repmat('-', 1, 92));

LtList = [10 20 50 100 200 500 1000];
fprintf('%-8s %-10s |', '单元', '方案');
fprintf('%9d', LtList);
fprintf('\n');

for it = 1:numel(types)
    for is = 1:numel(schemes)
        fprintf('%-8s %-10s |', types(it), schemes(is));
        for Lt = LtList
            v = T.wcD_qL4(T.elementType == types(it) & T.scheme == schemes(is) ...
                          & T.Lt == Lt);
            if isempty(v) || isnan(v(1))
                fprintf('%9s', '-');
            else
                fprintf('%9.5f', v(1));
            end
        end
        fprintf('\n');
    end
    fprintf('%s\n', repmat('-', 1, 92));
end

fprintf('图已保存到:\n  %s\n', fullfile(outDir, 'fig1_plate.png'));
