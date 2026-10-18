#pragma once

#include "feml/core/types.hpp"
#include "feml/element/integration_rule.hpp"

#include <Eigen/Core>

#include <string>
#include <string_view>
#include <vector>

namespace feml {

	struct Material;
	struct Section;

	/// 单元计算所需的全部输入数据。

	struct ElementContext {
		/// 单元各节点坐标，每行一个节点，列数为空间维数。
		/// 梁：n×1；平面：n×2；实体：n×3。
		Eigen::MatrixXd node_coords;

		const Material* material = nullptr;  ///< 材料（E、ν）
		const Section* section = nullptr;  ///< 截面（A、I、剪切修正因子）

		IntegrationScheme scheme = IntegrationScheme::Full;

		/// 作用在单元上的横向均布载荷，单位 N/m²。
		/// 板单元在 loadVector() 中使用；梁单元忽略此字段。
		Real transverse_load = 0.0;

		/// 单元内的节点位移（仅在后处理 recover() 时需要）
		Eigen::VectorXd element_dofs;
	};

	/// 单元后处理结果的输出容器
	struct ElementResult {
		Eigen::MatrixXd strain;        ///< 积分点上的应变（每行一个积分点）
		Eigen::MatrixXd stress;        ///< 积分点上的应力（每行一个积分点）
		Eigen::MatrixXd gauss_coords;  ///< 积分点的自然坐标（每行一个积分点）
	};

	/// 有限元单元的抽象基类
	///
	/// 新增一种单元只需要：
	///   1. 派生一个类，实现下面的纯虚函数；
	///   2. 在注册表里登记名字（见 element/registry.hpp）。
	/// 不需要修改程序的其他任何文件。
	class Element {
	public:
		/// 虚析构
		virtual ~Element() = default;

		// ---- 单元的基本信息 ------------------------------------------------

		/// 输入文件中使用的类型名，如 "BEAM2"、"QUAD4"
		virtual std::string_view name() const = 0;

		/// 节点个数
		virtual int nodeCount() const = 0;

		/// 每个节点的自由度数（梁 = 2: w、θ；平面 = 2: u、v；实体 = 3）
		virtual int dofPerNode() const = 0;

		/// 空间维数（梁 = 1，平面 = 2，实体 = 3）
		virtual int spatialDim() const = 0;

		/// 各自由度的名字，用于结果输出表头，如 {"W", "THETA"}
		virtual std::vector<std::string> dofNames() const = 0;

		/// 单元总自由度数
		int totalDof() const { return nodeCount() * dofPerNode(); }

		// ---- 核心计算 ------------------------------------------------------

		/// 计算单元刚度矩阵 Ke（尺寸 totalDof × totalDof）
		virtual void stiffness(const ElementContext& ctx, Eigen::MatrixXd& Ke) const = 0;

		/// 计算等效节点载荷向量 Fe（尺寸 totalDof）。
		/// 没有分布载荷的单元不必重写（默认返回零向量）。
		virtual void loadVector(const ElementContext& /*ctx*/, Eigen::VectorXd& Fe) const {
			Fe = Eigen::VectorXd::Zero(totalDof());
		}

		/// 后处理：由单元位移求积分点上的应变与应力。
		/// 暂时不做后处理的单元不必重写（默认返回空结果）。
		virtual void recover(const ElementContext& /*ctx*/, ElementResult& out) const {
			out = ElementResult{};
		}
	};

} // namespace feml