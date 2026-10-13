#include "feml/element/element.hpp"
#include "feml/element/registry.hpp"
#include "feml/model/material.hpp"
#include "feml/model/section.hpp"

#include "feml/core/exceptions.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <Eigen/Core>

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

using Catch::Matchers::WithinAbs;

namespace {

	/// 测试单元：2 节点线性杆单元

	class Bar2TestElement final : public feml::Element {
	public:
		std::string_view name() const override { return "BAR2_TEST"; }
		int nodeCount() const override { return 2; }
		int dofPerNode() const override { return 1; }
		int spatialDim() const override { return 1; }
		std::vector<std::string> dofNames() const override { return { "U" }; }

		void stiffness(const feml::ElementContext& ctx, Eigen::MatrixXd& Ke) const override {
			const double x1 = ctx.node_coords(0, 0);
			const double x2 = ctx.node_coords(1, 0);
			const double L = std::abs(x2 - x1);

			const double EA = ctx.material->E * ctx.section->area;
			const double k = EA / L;

			Ke.resize(2, 2);
			Ke << k, -k,
				-k, k;
		}
	};

	/// 构造一个长度为 2、EA = 600 的杆单元上下文（k = EA/L = 300）
	feml::ElementContext makeBarContext(feml::Material& mat, feml::Section& sec) {
		mat.E = 200.0;
		mat.nu = 0.3;
		sec.area = 3.0;

		feml::ElementContext ctx;
		ctx.node_coords.resize(2, 1);
		ctx.node_coords << 0.0, 2.0;
		ctx.material = &mat;
		ctx.section = &sec;
		return ctx;
	}

} // namespace

// ---------------------------------------------------------------------------
//  接口本身
// ---------------------------------------------------------------------------

TEST_CASE("a derived element reports its basic information", "[element]") {
	const Bar2TestElement bar;

	REQUIRE(bar.nodeCount() == 2);
	REQUIRE(bar.dofPerNode() == 1);
	REQUIRE(bar.spatialDim() == 1);
	REQUIRE(bar.name() == "BAR2_TEST");
	REQUIRE(bar.dofNames().size() == 1);

	// totalDof() 是非虚的便利函数，由 nodeCount() * dofPerNode() 得到
	REQUIRE(bar.totalDof() == 2);
}

TEST_CASE("bar element produces the textbook stiffness matrix", "[element]") {
	feml::Material mat;
	feml::Section sec;
	const feml::ElementContext ctx = makeBarContext(mat, sec);

	const Bar2TestElement bar;
	Eigen::MatrixXd Ke;
	bar.stiffness(ctx, Ke);

	const double k = mat.E * sec.area / 2.0;   // EA/L = 200 * 3 / 2 = 300

	REQUIRE(Ke.rows() == 2);
	REQUIRE(Ke.cols() == 2);
	REQUIRE_THAT(Ke(0, 0), WithinAbs(k, 1e-12));
	REQUIRE_THAT(Ke(0, 1), WithinAbs(-k, 1e-12));
	REQUIRE_THAT(Ke(1, 0), WithinAbs(-k, 1e-12));
	REQUIRE_THAT(Ke(1, 1), WithinAbs(k, 1e-12));
}

TEST_CASE("bar stiffness matrix is symmetric", "[element]") {
	feml::Material mat;
	feml::Section sec;
	const feml::ElementContext ctx = makeBarContext(mat, sec);

	const Bar2TestElement bar;
	Eigen::MatrixXd Ke;
	bar.stiffness(ctx, Ke);

	REQUIRE((Ke - Ke.transpose()).norm() < 1e-12);
}

TEST_CASE("bar stiffness offers no resistance to rigid body translation", "[element]") {
	feml::Material mat;
	feml::Section sec;
	const feml::ElementContext ctx = makeBarContext(mat, sec);

	const Bar2TestElement bar;
	Eigen::MatrixXd Ke;
	bar.stiffness(ctx, Ke);

	// 两个节点同向平移同样的距离 = 刚体运动，不产生任何节点力
	Eigen::Vector2d rigid;
	rigid << 1.0, 1.0;

	REQUIRE((Ke * rigid).norm() < 1e-12);
}

TEST_CASE("default loadVector returns a zero vector", "[element]") {
	feml::Material mat;
	feml::Section sec;
	const feml::ElementContext ctx = makeBarContext(mat, sec);

	const Bar2TestElement bar;
	Eigen::VectorXd Fe;
	bar.loadVector(ctx, Fe);   // 未重写，走基类的默认实现

	REQUIRE(Fe.size() == bar.totalDof());
	REQUIRE(Fe.isZero());
}

// ---------------------------------------------------------------------------
//  注册表
// ---------------------------------------------------------------------------

TEST_CASE("registry can add, query and create elements", "[element][registry]") {
	feml::ElementRegistry::add("BAR2_TEST", [] {
		return std::make_unique<Bar2TestElement>();
		});

	REQUIRE(feml::ElementRegistry::contains("BAR2_TEST"));
	REQUIRE(feml::ElementRegistry::contains("bar2_test"));   // 大小写不敏感

	const std::unique_ptr<feml::Element> e =
		feml::ElementRegistry::create("bar2_TEST");
	REQUIRE(e != nullptr);
	REQUIRE(e->name() == "BAR2_TEST");
	REQUIRE(e->totalDof() == 2);

	const std::vector<std::string> all = feml::ElementRegistry::names();
	REQUIRE(std::find(all.begin(), all.end(), "BAR2_TEST") != all.end());
}

TEST_CASE("registry rejects unknown element names", "[element][registry]") {
	REQUIRE_FALSE(feml::ElementRegistry::contains("NO_SUCH_ELEMENT"));

	REQUIRE_THROWS_AS(feml::ElementRegistry::create("NO_SUCH_ELEMENT"),
		feml::InputError);
}

TEST_CASE("registry rejects duplicate registration", "[element][registry]") {
	const auto factory = [] { return std::make_unique<Bar2TestElement>(); };

	feml::ElementRegistry::add("DUP_TEST", factory);

	// 大小写不同也视为同一个名字
	REQUIRE_THROWS_AS(feml::ElementRegistry::add("dup_test", factory),
		feml::InputError);
}