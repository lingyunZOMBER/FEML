%% ========================================================================
%  作业 3：Mindlin 板单元 —— 批量扫描结果绘图
%
%  【数据来源】
%      C++ 扫描驱动输出 plate_sweep.csv。在仓库根目录运行：
%          out\build\x64-Debug\bin\plate_sweep.exe examples\plate_sweep\plate_sweep.csv
%
%  【算例】
%      四边固支方板，边长 a，厚 t，受均布横向载荷 q
%      参考解：薄板理论 w = 0.00126·q·a⁴/D
%
%  【输出】
%      fig1_plate.png   六个面板：单元类型 × 积分方案，曲线为网格密度
% =========================================================================

clear; clc; close all;

csvFile = fullfile(fileparts(mfilename('fullpath')), 'plate_sweep.csv');
if ~isfile(csvFile)
    error(['找不到 %s\n' ...
           '请先在仓库根目录运行:\n' ...
           '  out\\build\\x64-Debug\\bin\\plate_sweep.exe examples\\plate_sweep\\plate_sweep.csv'], ...
           csvFile);
end

T = readtable(csvFile, 'TextType', 'string');
outDir = fileparts(mfilename('fullpath'));

types   = ["PLATE9", "PLATE8"];
schemes = ["FULL", "REDUCED", "SELECTIVE"];
meshes  = unique(T.mesh);

fprintf('读入 %d 行数据\n', height(T));
fprintf('  单元类型: %s\n', strjoin(unique(T.elementType)', ', '));
fprintf('  积分方案: %s\n', strjoin(unique(T.scheme)', ', '));
fprintf('  网格    : %s\n\n', strjoin(string(meshes)', ', '));

%% ---- 图：2 行（单元类型）× 3 列（积分方案）----

fig = figure('Name', 'Plate locking', 'Color', 'w', ...
             'Position', [100 100 1250 780]);
tl = tiledlayout(fig, 2, 3, 'TileSpacing', 'compact', 'Padding', 'compact');

colors = lines(numel(meshes));
lastAx = [];

for it = 1:numel(types)
    for is = 1:numel(schemes)
        lastAx = nexttile(tl);
        hold on;

        S = T(T.elementType == types(it) & T.scheme == schemes(is), :);

        for im = 1:numel(meshes)
            sub = S(S.mesh == meshes(im), :);
            if isempty(sub), continue; end
            sub = sortrows(sub, 'tOverA');

            plot(sub.tOverA, sub.ratio, '-o', ...
                 'Color', colors(im,:), 'LineWidth', 1.4, ...
                 'MarkerSize', 4, 'MarkerFaceColor', 'w', ...
                 'DisplayName', sprintf('%d×%d', meshes(im), meshes(im)));
        end

        set(gca, 'XScale', 'log', 'XDir', 'reverse');
        xlabel('t / a', 'FontSize', 10);
        ylabel('w_{中心} / w_{薄板}', 'FontSize', 10);
        title(sprintf('%s + %s', types(it), schemes(is)), 'FontSize', 11);
        grid on; box on;
        ylim([0 1.6]);
        yline(1.0, 'k:', 'HandleVisibility', 'off');
    end
end

title(tl, '四边固支方板：中心挠度比随厚跨比的变化（虚线为薄板理论解）', 'FontSize', 13);

lgd = legend(lastAx, 'Orientation', 'horizontal', 'NumColumns', 3, 'FontSize', 9);
lgd.Layout.Tile = 'south';

exportgraphics(fig, fullfile(outDir, 'fig1_plate.png'), 'Resolution', 200);

%% ---- 文字汇总 ----

fprintf('%s\n', repmat('=', 1, 84));
fprintf('ratio = w_center / w_thin（四边固支方板，均布载荷）\n');
fprintf('%s\n', repmat('-', 1, 84));

tl_list = [0.1, 0.05, 0.02, 0.01, 0.005, 0.001];
fprintf('%-8s %-4s %-10s |', '单元', '网格', '方案');
for t = tl_list
    fprintf('%9.3f', t);
end
fprintf('\n');

for it = 1:numel(types)
    for im = 1:numel(meshes)
        for is = 1:numel(schemes)
            fprintf('%-8s %-4d %-10s |', types(it), meshes(im), schemes(is));
            for t = tl_list
                v = T.ratio(T.elementType == types(it) & T.mesh == meshes(im) ...
                            & T.scheme == schemes(is) ...
                            & abs(T.tOverA - t) < 1e-12);
                if isempty(v) || isnan(v(1))
                    fprintf('%9s', '-');
                else
                    fprintf('%9.4f', v(1));
                end
            end
            fprintf('\n');
        end
    end
end
fprintf('%s\n\n', repmat('=', 1, 84));

fprintf('图已保存到:\n  %s\n', fullfile(outDir, 'fig1_plate.png'));
