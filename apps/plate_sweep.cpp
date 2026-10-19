/// ===========================================================================
///  plate_sweep —— Mindlin 板单元锁死试验（作业 3，对应课本图 10.12）
///
///  【程序目标】
///  用 8×8 网格离散四边固支方板，扫描长厚比，把中心挠度写入 CSV。
///
///  【用法】
///      plate_sweep [输出文件.csv]
///      省略参数时输出到 plate_sweep.csv
///
///  【算例】
///  四边固支方板，边长 L，厚 t，受均布横向载荷 q。
///  网格：每边 8 个单元。
///  参考解：精确薄板解 w_c = 0.00126 · q L⁴ / D，D = E t³ / (12(1−ν²))
///
///  【扫描范围】共 126 个算例
///      单元类型    PLATE9（9 节点 Lagrange）/ PLATE8（8 节点 Serendipity）
///      积分方案    FULL / REDUCED / SELECTIVE
///      长厚比 L/t  10, 12, 15, 20, 25, 30, 40, 50, 65, 80,
///                  100, 130, 160, 200, 250, 320, 400, 500, 650, 800, 1000
///
///  【输出 CSV 列】
///      elementType            单元类型
///      scheme                 积分方案
///      Lt                     长厚比 L/t
///      wcD_qL4                数值解，无量纲中心挠度 w_c·D/(q·L⁴)
///      wcD_qL4_thin           精确薄板解（常数 0.00126）
///
///  【单位制】SI：长度 m，力 N，弹性模量 Pa
/// ===========================================================================

#include "feml/analysis/static_analysis.hpp"
#include "feml/element/register_builtin.hpp"
#include "feml/linalg/eigen_solver.hpp"
#include "feml/model/dof_manager.hpp"
#include "feml/model/material.hpp"
#include "feml/model/mesh.hpp"
#include "feml/model/section.hpp"

#include <Eigen/Core>

#include <cmath>
#include <cstddef>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

namespace {

// ---------------------------------------------------------------------------
//  算例参数
// ---------------------------------------------------------------------------

constexpr double kE     = 200.0e9;   ///< 弹性模量 [Pa]
constexpr double kNu    = 0.3;       ///< 泊松比
constexpr double kL     = 1.0;       ///< 方板边长 [m]
constexpr double kQ     = 1000.0;    ///< 均布载荷 [N/m²]
constexpr double kShear = 5.0 / 6.0; ///< 剪切修正因子

constexpr int kMeshPerSide = 8;      ///< 每边单元数

/// 四边固支方板中心挠度的精确薄板解系数：w_c = α·q·L⁴/D
constexpr double kThinPlateAlpha = 0.00126;

// ---------------------------------------------------------------------------
//  网格
// ---------------------------------------------------------------------------

/// 方形板网格及其辅助信息
struct PlateMesh {
	feml::Mesh mesh;
	int nodesPerSide = 0;   ///< 每边节点数
	std::vector<int> id;    ///< 网格位置 (i,j) → 节点号；-1 表示不存在
	int centerNode = -1;    ///< 板中心节点号
};

/// 生成 n×n 个板单元的方形网格
///
/// 节点按 (2n+1)×(2n+1) 的规则网格排布。
/// PLATE8 不使用单元中心点，因此跳过 (奇数, 奇数) 位置的节点。
PlateMesh buildSquarePlate(const std::string& type, int n, double L) {
	PlateMesh pm;

	const int m = 2 * n + 1;
	const std::size_t ms = static_cast<std::size_t>(m);

	pm.nodesPerSide = m;
	pm.id.assign(ms * ms, -1);

	const bool quadratic = (type == "PLATE9");

	// ---- 节点 ----
	std::vector<double> xs;
	std::vector<double> ys;
	int count = 0;

	for (std::size_t j = 0; j < ms; ++j) {
		for (std::size_t i = 0; i < ms; ++i) {
			if (!quadratic && (i % 2 == 1) && (j % 2 == 1)) {
				continue;   // PLATE8 无单元中心节点
			}
			pm.id[j * ms + i] = count++;

			xs.push_back(L * static_cast<double>(i) / static_cast<double>(m - 1));
			ys.push_back(L * static_cast<double>(j) / static_cast<double>(m - 1));
		}
	}

	pm.mesh.node_coords.resize(count, 2);
	for (std::size_t k = 0; k < xs.size(); ++k) {
		pm.mesh.node_coords(static_cast<Eigen::Index>(k), 0) = xs[k];
		pm.mesh.node_coords(static_cast<Eigen::Index>(k), 1) = ys[k];
	}

	// ---- 单元 ----
	// 自然坐标编号对应的网格偏移（与 plate_shape.hpp 的编号一致）
	const int offset[9][2] = {
		{0, 0}, {2, 0}, {2, 2}, {0, 2},   // 角节点
		{1, 0}, {2, 1}, {1, 2}, {0, 1},   // 边中点
		{1, 1}                            // 中心（仅 PLATE9）
	};

	const std::size_t nNode = quadratic ? 9 : 8;

	for (int ey = 0; ey < n; ++ey) {
		for (int ex = 0; ex < n; ++ex) {
			const int i0 = 2 * ex;
			const int j0 = 2 * ey;

			std::vector<feml::NodeId> conn;
			conn.reserve(nNode);

			for (std::size_t k = 0; k < nNode; ++k) {
				const int gi = i0 + offset[k][0];
				const int gj = j0 + offset[k][1];

				const int node = pm.id[static_cast<std::size_t>(gj) * ms
				                       + static_cast<std::size_t>(gi)];

				conn.push_back(static_cast<feml::NodeId>(node));
			}

			pm.mesh.element_type.push_back(type);
			pm.mesh.element_nodes.push_back(std::move(conn));
		}
	}

	pm.centerNode = pm.id[static_cast<std::size_t>(n) * ms + static_cast<std::size_t>(n)];

	return pm;
}

// ---------------------------------------------------------------------------
//  求解
// ---------------------------------------------------------------------------

/// 求解四边固支方板，返回中心挠度
double solveClampedPlate(const PlateMesh& pm, double thickness,
                         feml::IntegrationScheme scheme) {
	feml::Material material;
	material.id = 1;
	material.E  = kE;
	material.nu = kNu;

	feml::Section section;
	section.id          = 1;
	section.thickness   = thickness;
	section.shearFactor = kShear;

	// 自由度与边界条件：四条边上所有节点的 3 个自由度全部约束
	feml::DofManager dofs(pm.mesh.nodeCount(), 3);

	const int m = pm.nodesPerSide;
	const std::size_t ms = static_cast<std::size_t>(m);

	for (std::size_t j = 0; j < ms; ++j) {
		for (std::size_t i = 0; i < ms; ++i) {
			const int node = pm.id[j * ms + i];
			if (node < 0) {
				continue;
			}

			const bool boundary = (i == 0 || i == ms - 1 || j == 0 || j == ms - 1);
			if (!boundary) {
				continue;
			}

			for (std::size_t c = 0; c < 3; ++c) {
				dofs.constrain(static_cast<feml::NodeId>(node),
				               static_cast<feml::DofComponent>(c));
			}
		}
	}

	dofs.number();

	feml::StaticProblem problem;
	problem.mesh           = &pm.mesh;
	problem.material       = &material;
	problem.section        = &section;
	problem.dofs           = &dofs;
	problem.scheme         = scheme;
	problem.transverseLoad = kQ;

	problem.nodalLoads = Eigen::VectorXd::Zero(3 * pm.mesh.nodeCount());

	feml::EigenLdltSolver solver;
	const feml::StaticResult result = feml::solveStatic(problem, solver);

	return result.displacement(3 * pm.centerNode);
}

// ---------------------------------------------------------------------------
//  参考解与名字
// ---------------------------------------------------------------------------

/// 无量纲化：w·D/(q·L⁴)，D = E t³/(12(1−ν²))
double dimensionless(double w, double thickness) {
	const double D = kE * thickness * thickness * thickness
	               / (12.0 * (1.0 - kNu * kNu));
	return w * D / (kQ * kL * kL * kL * kL);
}

const char* schemeName(feml::IntegrationScheme scheme) {
	switch (scheme) {
	case feml::IntegrationScheme::Full:             return "FULL";
	case feml::IntegrationScheme::Reduced:          return "REDUCED";
	case feml::IntegrationScheme::SelectiveReduced: return "SELECTIVE";
	}
	return "UNKNOWN";
}

} // namespace

// ---------------------------------------------------------------------------
//  主程序
// ---------------------------------------------------------------------------

int main(int argc, char** argv) {
	try {
		const std::string outPath = (argc > 1) ? argv[1] : "plate_sweep.csv";

		feml::registerBuiltinElements();

		std::ofstream out(outPath);
		if (!out) {
			std::cerr << "无法创建输出文件: " << outPath << "\n";
			return 1;
		}

		out << std::scientific << std::setprecision(15);
		out << "elementType,scheme,Lt,wcD_qL4,wcD_qL4_thin\n";

		// 扫描参数
		const feml::IntegrationScheme schemes[] = {
			feml::IntegrationScheme::Full,
			feml::IntegrationScheme::Reduced,
			feml::IntegrationScheme::SelectiveReduced
		};
		const double lengthRatios[] = {
			10.0, 12.0, 15.0, 20.0, 25.0, 30.0, 40.0, 50.0, 65.0, 80.0,
			100.0, 130.0, 160.0, 200.0, 250.0, 320.0, 400.0, 500.0, 650.0, 800.0, 1000.0
		};

		// 两种单元的 8×8 网格，各建一次
		const PlateMesh pm9 = buildSquarePlate("PLATE9", kMeshPerSide, kL);
		const PlateMesh pm8 = buildSquarePlate("PLATE8", kMeshPerSide, kL);

		const PlateMesh* meshes[] = { &pm9, &pm8 };
		const char* const types[] = { "PLATE9", "PLATE8" };

		int written = 0;
		int failed  = 0;

		for (std::size_t t = 0; t < 2; ++t) {
			for (const feml::IntegrationScheme scheme : schemes) {
				for (const double Lt : lengthRatios) {

					const double thickness = kL / Lt;

					try {
						const double wc = solveClampedPlate(*meshes[t], thickness, scheme);
						const double wcDim = dimensionless(wc, thickness);

						out << types[t] << ','
						    << schemeName(scheme) << ','
						    << Lt << ','
						    << wcDim << ','
						    << kThinPlateAlpha << '\n';

						++written;
					}
					catch (const std::exception& e) {
						std::cerr << "算例失败: " << types[t] << " " << schemeName(scheme)
						          << " L/t=" << Lt << " — " << e.what() << "\n";

						out << types[t] << ','
						    << schemeName(scheme) << ','
						    << Lt << ','
						    << "nan,nan\n";

						++failed;
					}
				}
			}
		}

		out.close();

		std::error_code ec;
		const auto absPath = std::filesystem::absolute(outPath, ec);

		std::cout << "扫描完成：" << written << " 个算例";
		if (failed > 0) {
			std::cout << "（其中 " << failed << " 个失败）";
		}
		std::cout << "\n输出文件：" << (ec ? outPath : absPath.string()) << "\n";

		return (failed == 0) ? 0 : 2;
	}
	catch (const std::exception& e) {
		std::cerr << "扫描中止: " << e.what() << "\n";
		return 1;
	}
}
