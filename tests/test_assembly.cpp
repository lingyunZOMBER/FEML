#include "feml/analysis/assembler.hpp"
#include "feml/analysis/static_analysis.hpp"
#include "feml/core/exceptions.hpp"
#include "feml/element/element.hpp"
#include "feml/element/register_builtin.hpp"
#include "feml/element/registry.hpp"
#include "feml/linalg/eigen_solver.hpp"
#include "feml/model/dof_manager.hpp"
#include "feml/model/material.hpp"
#include "feml/model/mesh.hpp"
#include "feml/model/section.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <Eigen/Core>

#include <cmath>
#include <memory>
#include <string>
#include <vector>

using Catch::Matchers::WithinRel;

namespace {

// 所有算例共用同一套材料和截面：矩形截面钢梁
constexpr double kE  = 200.0e9;
constexpr double kNu = 0.3;
constexpr double kWidth = 0.1;    // 截面宽 b

/// 生成 n 个等长 BEAM2 单元组成的悬臂梁网格
feml::Mesh makeCantileverMesh(int elementCount, double L) {
	feml::Mesh mesh;

	const int nodeCount = elementCount + 1;
	mesh.node_coords.resize(nodeCount, 1);
	for (int i = 0; i < nodeCount; ++i) {
		mesh.node_coords(i, 0) = L * static_cast<double>(i)
		                       / static_cast<double>(elementCount);
	}

	mesh.element_type.assign(static_cast<std::size_t>(elementCount), "BEAM2");
	mesh.element_nodes.resize(static_cast<std::size_t>(elementCount));
	for (int e = 0; e < elementCount; ++e) {
		mesh.element_nodes[static_cast<std::size_t>(e)] =
			std::vector<feml::NodeId>{ static_cast<feml::NodeId>(e),
			                           static_cast<feml::NodeId>(e + 1) };
	}

	return mesh;
}

/// 与网格配套的材料和截面（矩形截面，k_s = 5/6）
feml::Material makeMaterial() {
	feml::Material m;
	m.id = 1;
	m.E  = kE;
	m.nu = kNu;
	return m;
}

feml::Section makeSection(double height) {
	feml::Section s;
	s.id          = 1;
	s.area        = kWidth * height;
	s.inertia     = kWidth * height * height * height / 12.0;
	s.shearFactor = 5.0 / 6.0;
	return s;
}

double bendingStiffness(const feml::Section& s) { return kE * s.inertia; }
double shearStiffness(const feml::Material& m, const feml::Section& s) {
	return s.shearFactor * m.shearModulus() * s.area;
}

/// 建一个悬臂梁问题：节点 0 固支（w = θ = 0）
feml::StaticProblem makeCantileverProblem(const feml::Mesh& mesh,
                                          const feml::Material& material,
                                          const feml::Section& section,
                                          const feml::DofManager& dofs,
                                          double tipLoad, double tipMoment) {
	const int lastNode = mesh.nodeCount() - 1;
	const int ndof = dofs.dofPerNode();

	feml::StaticProblem problem;
	problem.mesh     = &mesh;
	problem.material = &material;
	problem.section  = &section;
	problem.dofs     = &dofs;
	problem.scheme   = feml::IntegrationScheme::Reduced;

	problem.nodalLoads = Eigen::VectorXd::Zero(ndof * mesh.nodeCount());
	problem.nodalLoads(lastNode * ndof + 0) = tipLoad;    // w
	problem.nodalLoads(lastNode * ndof + 1) = tipMoment;  // θ

	return problem;
}

} // namespace

// ===========================================================================
//  DofManager：自由度映射
// ===========================================================================

TEST_CASE("DofManager numbers the free degrees of freedom in node-major order",
          "[dof]") {
	feml::DofManager dofs(3, 2);   // 3 个节点，每节点 2 个自由度

	// 节点 0 两个自由度全约束（固支端）
	dofs.constrain(0, 0);
	dofs.constrain(0, 1);
	// 节点 1 只约束 w
	dofs.constrain(1, 0);

	dofs.number();

	REQUIRE(dofs.nodeCount() == 3);
	REQUIRE(dofs.dofPerNode() == 2);
	REQUIRE(dofs.totalDof() == 6);

	// 6 个自由度减去 3 个约束 = 3 个方程
	REQUIRE(dofs.equationCount() == 3);

	// 被约束的返回 KInvalidIndex
	REQUIRE(dofs.equationNumber(0, 0) == feml::KInvalidIndex);
	REQUIRE(dofs.equationNumber(0, 1) == feml::KInvalidIndex);
	REQUIRE(dofs.equationNumber(1, 0) == feml::KInvalidIndex);

	// 自由的按"节点优先、分量其次"编号
	REQUIRE(dofs.equationNumber(1, 1) == 0);
	REQUIRE(dofs.equationNumber(2, 0) == 1);
	REQUIRE(dofs.equationNumber(2, 1) == 2);

	// 反查
	REQUIRE(dofs.nodeOf(0) == 1);
	REQUIRE(dofs.componentOf(0) == 1);
	REQUIRE(dofs.nodeOf(2) == 2);
	REQUIRE(dofs.componentOf(2) == 1);
}

TEST_CASE("DofManager rejects constraints applied after numbering", "[dof]") {
	feml::DofManager dofs(2, 2);
	dofs.constrain(0, 0);
	dofs.number();

	// 编号依赖约束集合，改约束就必须重新编号。
	// 静默允许会导致方程号和实际约束不一致——这类错误极难查。
	REQUIRE_THROWS_AS(dofs.constrain(1, 0), feml::SolverError);
}

TEST_CASE("DofManager rejects a fully constrained system", "[dof]") {
	feml::DofManager dofs(1, 2);
	dofs.constrain(0, 0);
	dofs.constrain(0, 1);

	// 一个方程都没有，是使用错误，不是"空解"
	REQUIRE_THROWS_AS(dofs.number(), feml::SolverError);
}

TEST_CASE("DofManager rejects out-of-range node and component", "[dof]") {
	feml::DofManager dofs(2, 2);

	REQUIRE_THROWS_AS(dofs.constrain(-1, 0),  feml::InputError);
	REQUIRE_THROWS_AS(dofs.constrain(2, 0),   feml::InputError);
	REQUIRE_THROWS_AS(dofs.constrain(0, -1),  feml::InputError);
	REQUIRE_THROWS_AS(dofs.constrain(0, 2),   feml::InputError);
}

// ===========================================================================
//  组装
// ===========================================================================

TEST_CASE("assembling a single element reproduces its element stiffness", "[assembly]") {
	feml::registerBuiltinElements();

	const double L = 2.0;
	const double height = 0.1;

	const feml::Mesh mesh = makeCantileverMesh(1, L);
	const feml::Material material = makeMaterial();
	const feml::Section section = makeSection(height);

	const Eigen::MatrixXd K = feml::assembleStiffness(
		mesh, material, section, feml::IntegrationScheme::Reduced);

	// 1 个单元 → 总刚就是单元刚度阵本身
	REQUIRE(K.rows() == 4);
	REQUIRE(K.cols() == 4);

	// 用同一个单元直接算一遍作对照
	const std::unique_ptr<feml::Element> element = feml::ElementRegistry::create("BEAM2");
	feml::ElementContext ctx;
	ctx.node_coords = mesh.node_coords;
	ctx.material    = &material;
	ctx.section     = &section;
	ctx.scheme      = feml::IntegrationScheme::Reduced;

	Eigen::MatrixXd Ke;
	element->stiffness(ctx, Ke);

	REQUIRE((K - Ke).norm() < Ke.norm() * 1e-14);
}

TEST_CASE("assembly rejects a mesh with inconsistent element dof counts",
          "[assembly]") {
	feml::registerBuiltinElements();

	feml::Mesh mesh = makeCantileverMesh(2, 2.0);
	mesh.element_type[1] = "NO_SUCH_ELEMENT";

	const feml::Material material = makeMaterial();
	const feml::Section section = makeSection(0.1);

	// 未知的单元类型应该在注册表那里就被挡下
	REQUIRE_THROWS_AS(
		feml::assembleStiffness(mesh, material, section,
		                        feml::IntegrationScheme::Reduced),
		feml::InputError);
}

// ===========================================================================
//  完整求解：多单元悬臂梁
// ===========================================================================

TEST_CASE("pure end moment is exact for any mesh with reduced integration",
          "[assembly][beam2]") {
	// 纯弯是"减缩积分精确"这个性质最强的验证：它与单元数无关。
	//
	// 原因：减缩积分每个单元强制 γ(0) = 0，即
	//     (w₂-w₁)/h = (θ₁+θ₂)/2      （平均斜率 = 平均转角）
	// 而纯弯的解析解 θ = Mx/EI、w = Mx²/(2EI) 恰好逐单元满足这条约束，
	// 所以离散解就是解析解。
	feml::registerBuiltinElements();

	const double L = 2.0;
	const double height = 0.1;
	const double M = 1000.0;

	const feml::Material material = makeMaterial();
	const feml::Section section = makeSection(height);

	const double EI = bendingStiffness(section);
	const double wExact = M * L * L / (2.0 * EI);
	const double tExact = M * L / EI;

	for (const int elementCount : { 1, 2, 3, 4, 8, 16 }) {
		const feml::Mesh mesh = makeCantileverMesh(elementCount, L);

		feml::DofManager dofs(mesh.nodeCount(), 2);
		dofs.constrain(0, 0);   // w
		dofs.constrain(0, 1);   // θ
		dofs.number();

		const feml::StaticProblem problem =
			makeCantileverProblem(mesh, material, section, dofs, 0.0, M);

		feml::EigenLdltSolver solver;
		const feml::StaticResult result = feml::solveStatic(problem, solver);

		const int lastNode = mesh.nodeCount() - 1;
		const int ndof = dofs.dofPerNode();

		INFO("elementCount = " << elementCount);
		REQUIRE_THAT(result.displacement(lastNode * ndof + 0), WithinRel(wExact, 1e-9));
		REQUIRE_THAT(result.displacement(lastNode * ndof + 1), WithinRel(tExact, 1e-9));
	}
}

TEST_CASE("single element under tip load matches the hand-derived formula",
          "[assembly][beam2]") {
	// 1 个单元 + 减缩积分的挠度是 PL/(kGA) + PL³/(4EI)：
	// 剪切项精确，弯曲项被积成 L³/4 而不是解析的 L³/3。
	feml::registerBuiltinElements();

	const double L = 2.0;
	const double height = 0.1;
	const double P = 1000.0;

	const feml::Mesh mesh = makeCantileverMesh(1, L);
	const feml::Material material = makeMaterial();
	const feml::Section section = makeSection(height);

	feml::DofManager dofs(mesh.nodeCount(), 2);
	dofs.constrain(0, 0);
	dofs.constrain(0, 1);
	dofs.number();

	const feml::StaticProblem problem =
		makeCantileverProblem(mesh, material, section, dofs, P, 0.0);

	feml::EigenLdltSolver solver;
	const feml::StaticResult result = feml::solveStatic(problem, solver);

	const double EI = bendingStiffness(section);
	const double kGA = shearStiffness(material, section);

	const double wElement = P * L / kGA + P * L * L * L / (4.0 * EI);
	const double tExact   = P * L * L / (2.0 * EI);   // 转角仍是精确的

	REQUIRE_THAT(result.displacement(2), WithinRel(wElement, 1e-10));
	REQUIRE_THAT(result.displacement(3), WithinRel(tExact, 1e-10));
}

TEST_CASE("tip load converges to the analytical solution as the mesh is refined",
          "[assembly][beam2]") {
	feml::registerBuiltinElements();

	const double L = 2.0;
	const double height = 0.1;
	const double P = 1000.0;

	const feml::Material material = makeMaterial();
	const feml::Section section = makeSection(height);

	const double EI = bendingStiffness(section);
	const double kGA = shearStiffness(material, section);
	const double wExact = P * L * L * L / (3.0 * EI) + P * L / kGA;

	double previousError = 1.0;

	for (const int elementCount : { 1, 2, 4, 8, 16, 32 }) {
		const feml::Mesh mesh = makeCantileverMesh(elementCount, L);

		feml::DofManager dofs(mesh.nodeCount(), 2);
		dofs.constrain(0, 0);
		dofs.constrain(0, 1);
		dofs.number();

		const feml::StaticProblem problem =
			makeCantileverProblem(mesh, material, section, dofs, P, 0.0);

		feml::EigenLdltSolver solver;
		const feml::StaticResult result = feml::solveStatic(problem, solver);

		const int lastNode = mesh.nodeCount() - 1;
		const double w = result.displacement(lastNode * 2 + 0);
		const double error = std::abs(w - wExact) / wExact;

		INFO("elementCount = " << elementCount << ", error = " << error);

		// 误差随加密单调减小
		REQUIRE(error < previousError);
		previousError = error;
	}

	// 32 个单元后误差应该很小（O(h²) 收敛）
	REQUIRE(previousError < 1e-3);
}

// ===========================================================================
//  支反力与求解器
// ===========================================================================

TEST_CASE("reactions balance the applied load", "[assembly]") {
	// 静态平衡：固支端的总支反力必须与外力大小相等、方向相反。
	// 这是个不依赖具体单元实现的整体检查。
	feml::registerBuiltinElements();

	const double L = 2.0;
	const double P = 1000.0;

	const feml::Mesh mesh = makeCantileverMesh(4, L);
	const feml::Material material = makeMaterial();
	const feml::Section section = makeSection(0.1);

	feml::DofManager dofs(mesh.nodeCount(), 2);
	dofs.constrain(0, 0);
	dofs.constrain(0, 1);
	dofs.number();

	const feml::StaticProblem problem =
		makeCantileverProblem(mesh, material, section, dofs, P, 0.0);

	feml::EigenLdltSolver solver;
	const feml::StaticResult result = feml::solveStatic(problem, solver);

	// 节点 0 的 w 分量支反力 = −P
	REQUIRE_THAT(result.reaction(0), WithinRel(-P, 1e-10));

	// 自由自由度上的"支反力"应该为零（浮点残差量级）——
	// 这顺便验证了 K·u 与 F 的一致性
	const int lastNode = mesh.nodeCount() - 1;
	REQUIRE(std::abs(result.reaction(lastNode * 2 + 0)) < 1e-6);
	REQUIRE(std::abs(result.reaction(lastNode * 2 + 1)) < 1e-6);
}

TEST_CASE("solver reports failure for a singular system", "[linalg]") {
	// 欠约束的结构会让总刚奇异。求解器必须【返回失败】而不是给出
	// 一堆巨大的、看起来像溢出的位移。
	feml::EigenLdltSolver ldlt;

	Eigen::MatrixXd K = Eigen::MatrixXd::Zero(2, 2);
	Eigen::VectorXd F = Eigen::VectorXd::Zero(2);
	Eigen::VectorXd u;

	REQUIRE_FALSE(ldlt.solve(K, F, u));   // 全零矩阵显然奇异

	Eigen::MatrixXd K2(2, 2);
	K2 << 1.0, 2.0,
	      2.0, 4.0;                        // 秩 1
	REQUIRE_FALSE(ldlt.solve(K2, F, u));
}

TEST_CASE("solver reports failure for mismatched dimensions", "[linalg]") {
	feml::EigenLdltSolver ldlt;

	const Eigen::MatrixXd K = Eigen::MatrixXd::Identity(3, 3);
	const Eigen::VectorXd F = Eigen::VectorXd::Zero(2);   // 长度不符
	Eigen::VectorXd u;

	// 尺寸不符是使用错误（抛异常），不是数值失败（返回 false）
	REQUIRE_THROWS_AS(ldlt.solve(K, F, u), feml::SolverError);
}
