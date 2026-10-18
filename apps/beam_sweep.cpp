/// ===========================================================================
///  beam_sweep —— Timoshenko 梁单元批量扫描驱动
///
///  【程序目标】
///  对一组参数组合逐一求解悬臂梁算例，把端部挠度与转角写入 CSV 文件。
///
///  【用法】
///      beam_sweep [输出文件.csv]
///      省略参数时输出到 beam_sweep.csv
///
///  【扫描范围】共 864 个算例
///      单元类型    BEAM2 / BEAM3
///      单元数      1, 2, 4, 8, 16, 32, 64, 128
///      积分方案    FULL / REDUCED / SELECTIVE
///      载荷类型    LOAD（端部集中力）/ MOMENT（端部弯矩）
///      细长比 h/L  0.5, 0.3, 0.2, 0.1, 0.05, 0.02, 0.01, 0.005, 0.001
///
///  【输出 CSV 列】
///      elementType            单元类型
///      elementCount           单元数
///      scheme                 积分方案
///      loadType               载荷类型
///      hOverL                 细长比 h/L
///      tipW                   端部挠度（数值解）
///      tipTheta               端部转角（数值解）
///
///  【单位制】SI：长度 m，力 N，弹性模量 Pa
///
///  【退出码】
///      0  全部算例成功
///      1  无法创建输出文件，或发生未捕获的异常
///      2  部分算例求解失败（对应行写入 nan）
/// ===========================================================================

#include "feml/analysis/static_analysis.hpp"
#include "feml/element/register_builtin.hpp"
#include "feml/linalg/eigen_solver.hpp"
#include "feml/model/dof_manager.hpp"
#include "feml/model/material.hpp"
#include "feml/model/mesh.hpp"
#include "feml/model/section.hpp"

#include <Eigen/Core>

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

constexpr double kE      = 200.0e9;   ///< 弹性模量 [Pa]
constexpr double kNu     = 0.3;       ///< 泊松比
constexpr double kL      = 1.0;       ///< 梁长 [m]
constexpr double kWidth  = 0.1;       ///< 截面宽 [m]
constexpr double kShear  = 5.0 / 6.0; ///< 剪切修正因子

constexpr double kTipLoad   = 1000.0; ///< 端部集中力 [N]
constexpr double kTipMoment = 1000.0; ///< 端部弯矩 [N·m]

// ---------------------------------------------------------------------------
//  网格生成
// ---------------------------------------------------------------------------

/// 生成等长梁单元组成的悬臂梁网格。
///
/// 节点沿 x 轴等距分布，x ∈ [0, kL]。
///
/// @param type         单元类型名："BEAM2" 或 "BEAM3"
/// @param elementCount 单元数
/// @return 网格对象：
///         BEAM2 → elementCount+1 个节点，每单元 2 个节点
///         BEAM3 → 2*elementCount+1 个节点，每单元 3 个节点
feml::Mesh makeBeamMesh(const std::string& type, int elementCount) {
	feml::Mesh mesh;

	const bool quadratic = (type == "BEAM3");
	const int nodesPerElement = quadratic ? 3 : 2;
	const int intervals = quadratic ? 2 * elementCount : elementCount;
	const int nodeCount = intervals + 1;

	// 节点坐标
	mesh.node_coords.resize(nodeCount, 1);
	for (int i = 0; i < nodeCount; ++i) {
		mesh.node_coords(i, 0) = kL * static_cast<double>(i)
		                       / static_cast<double>(intervals);
	}

	// 单元类型：所有单元相同
	mesh.element_type.assign(static_cast<std::size_t>(elementCount), type);

	// 单元连接：BEAM2 取 (e, e+1)，BEAM3 取 (2e, 2e+1, 2e+2)
	mesh.element_nodes.resize(static_cast<std::size_t>(elementCount));

	for (int e = 0; e < elementCount; ++e) {
		const int first = quadratic ? 2 * e : e;

		std::vector<feml::NodeId> conn;
		conn.reserve(static_cast<std::size_t>(nodesPerElement));
		for (int k = 0; k < nodesPerElement; ++k) {
			conn.push_back(static_cast<feml::NodeId>(first + k));
		}
		mesh.element_nodes[static_cast<std::size_t>(e)] = std::move(conn);
	}

	return mesh;
}

// ---------------------------------------------------------------------------
//  数值求解
// ---------------------------------------------------------------------------

/// 端部节点的数值解
struct TipValues {
	double w     = 0.0;   ///< 端部挠度
	double theta = 0.0;   ///< 端部转角
};

/// 求解单个悬臂梁算例。
///
/// 边界条件：节点 0 固支（w = 0，θ = 0）
/// 载荷：最后一个节点施加集中力与弯矩
///
/// @return 端部节点的挠度与转角
/// @throws 求解失败时抛出异常（由调用方捕获）
TipValues solveCantilever(const feml::Mesh& mesh,
                          const feml::Material& material,
                          const feml::Section& section,
                          feml::IntegrationScheme scheme,
                          double tipLoad, double tipMoment) {
	// 自由度与约束
	feml::DofManager dofs(mesh.nodeCount(), 2);
	dofs.constrain(0, 0);   // w
	dofs.constrain(0, 1);   // θ
	dofs.number();

	// 问题装配
	feml::StaticProblem problem;
	problem.mesh     = &mesh;
	problem.material = &material;
	problem.section  = &section;
	problem.dofs     = &dofs;
	problem.scheme   = scheme;

	// 载荷向量：
	const int last = mesh.nodeCount() - 1;
	problem.nodalLoads = Eigen::VectorXd::Zero(2 * mesh.nodeCount());
	problem.nodalLoads(last * 2 + 0) = tipLoad;
	problem.nodalLoads(last * 2 + 1) = tipMoment;

	// 求解
	feml::EigenLdltSolver solver;
	const feml::StaticResult result = feml::solveStatic(problem, solver);

	return TipValues{ result.displacement(last * 2 + 0),
	                  result.displacement(last * 2 + 1) };
}

// ---------------------------------------------------------------------------
/// 积分方案 → CSV 中使用的字符串
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
		// 输出文件路径
		const std::string outPath = (argc > 1) ? argv[1] : "beam_sweep.csv";

		// 注册内置单元：BEAM2、BEAM3
		feml::registerBuiltinElements();

		std::ofstream out(outPath);
		if (!out) {
			std::cerr << "无法创建输出文件: " << outPath << "\n";
			return 1;
		}

		// CSV 输出格式：科学计数法，15 位有效数字
		out << std::scientific << std::setprecision(15);

		// 表头
		out << "elementType,elementCount,scheme,loadType,hOverL,tipW,tipTheta\n";

		// 扫描参数表
		const char* const elementTypes[] = { "BEAM2", "BEAM3" };
		const int elementCounts[]        = { 1, 2, 4, 8, 16, 32, 64, 128};
		const feml::IntegrationScheme schemes[] = {
			feml::IntegrationScheme::Full,
			feml::IntegrationScheme::Reduced,
			feml::IntegrationScheme::SelectiveReduced
		};
		const char* const loadTypes[] = { "LOAD", "MOMENT" };
		const double slenderness[] = {
			0.5, 0.3, 0.2, 0.1, 0.05, 0.02, 0.01, 0.005, 0.001
		};

		int written = 0;   // 成功算例数
		int failed  = 0;   // 失败算例数

		// 五层循环：单元类型 → 单元数 → 积分方案 → 载荷类型 → 细长比
		for (const char* type : elementTypes) {
			for (const int elementCount : elementCounts) {
				for (const feml::IntegrationScheme scheme : schemes) {
					for (const char* loadType : loadTypes) {
						for (const double hOverL : slenderness) {

							// 由细长比确定截面尺寸（矩形截面）
							const double h = hOverL * kL;
							const double area    = kWidth * h;
							const double inertia = kWidth * h * h * h / 12.0;

							// 材料
							feml::Material material;
							material.id = 1;
							material.E  = kE;
							material.nu = kNu;

							// 截面
							feml::Section section;
							section.id          = 1;
							section.area        = area;
							section.inertia     = inertia;
							section.shearFactor = kShear;

							// 网格
							const feml::Mesh mesh = makeBeamMesh(type, elementCount);

							// 载荷：二选一
							const bool isLoad = (std::string(loadType) == "LOAD");
							const double tipLoad   = isLoad ? kTipLoad   : 0.0;
							const double tipMoment = isLoad ? 0.0       : kTipMoment;

							try {
								// 数值解
								const TipValues tip = solveCantilever(
									mesh, material, section, scheme, tipLoad, tipMoment);

								// 写一行
								out << type << ','
								    << elementCount << ','
								    << schemeName(scheme) << ','
								    << loadType << ','
								    << hOverL << ','
								    << tip.w << ','
								    << tip.theta << '\n';

								++written;
							}
							catch (const std::exception& e) {
								// 单个算例失败：写入 nan 并记录，继续扫描
								std::cerr << "算例失败: " << type << " n=" << elementCount
								          << " " << schemeName(scheme) << " " << loadType
								          << " h/L=" << hOverL << " — " << e.what() << "\n";

								out << type << ','
								    << elementCount << ','
								    << schemeName(scheme) << ','
								    << loadType << ','
								    << hOverL << ','
								    << "nan,nan\n";

								++failed;
							}
						}
					}
				}
			}
		}

		out.close();

		// 输出摘要
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
