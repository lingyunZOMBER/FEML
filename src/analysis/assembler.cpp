#include "feml/analysis/assembler.hpp"

#include "feml/core/exceptions.hpp"
#include "feml/element/element.hpp"
#include "feml/element/registry.hpp"
#include "feml/model/material.hpp"
#include "feml/model/section.hpp"

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace feml {
namespace {

/// 单元的第 k 个节点、第 c 个分量 → 全局自由度下标
int globalDof(const std::vector<NodeId>& elementNodes, int dofPerNode,
              std::size_t k, int c) {
	return static_cast<int>(elementNodes[k]) * dofPerNode + c;
}

/// 数据自洽性检查。全部在循环之前做完，避免算到一半才发现问题。
void validateMesh(const Mesh& mesh) {
	if (static_cast<int>(mesh.element_nodes.size()) != mesh.elementCount()) {
		throw InputError("element_type 与 element_nodes 的长度不一致："
		                 + std::to_string(mesh.elementCount()) + " vs "
		                 + std::to_string(mesh.element_nodes.size()));
	}
	if (mesh.nodeCount() <= 0) {
		throw InputError("网格里没有节点");
	}
	if (mesh.elementCount() <= 0) {
		throw InputError("网格里没有单元");
	}
	if (mesh.spatialDim() <= 0) {
		throw InputError("节点坐标的维数必须为正");
	}
}

/// 由单元类型确定每节点自由度数，并校验所有单元一致(不考虑混合网格，会让组装变的过于复杂)
int determineDofPerNode(const Mesh& mesh) {
	int dofPerNode = -1;

	for (std::size_t e = 0; e < mesh.element_type.size(); ++e) {
		const std::unique_ptr<Element> element =
			ElementRegistry::create(mesh.element_type[e]);

		const int d = element->dofPerNode();
		if (dofPerNode < 0) {
			dofPerNode = d;
		} else if (d != dofPerNode) {
			throw InputError("单元 " + std::to_string(e) + " 的每节点自由度数为 "
			                 + std::to_string(d) + "，与前面的 "
			                 + std::to_string(dofPerNode)
			                 + " 不一致。当前实现要求所有单元的自由度数相同");
		}
	}

	return dofPerNode;
}

/// 摘出某个单元的节点坐标
Eigen::MatrixXd gatherNodeCoords(const Mesh& mesh, const std::vector<NodeId>& nodes,
                                 int spatialDim, int elementIndex) {
	Eigen::MatrixXd coords(static_cast<Eigen::Index>(nodes.size()), spatialDim);

	for (std::size_t k = 0; k < nodes.size(); ++k) {
		const NodeId id = nodes[k];
		if (id < 0 || id >= mesh.nodeCount()) {
			throw InputError("单元 " + std::to_string(elementIndex)
			                 + " 引用了不存在的节点 " + std::to_string(id));
		}
		coords.row(static_cast<Eigen::Index>(k)) = mesh.node_coords.row(id);
	}

	return coords;
}

} // namespace

Eigen::MatrixXd assembleStiffness(const Mesh& mesh,
                                  const Material& material,
                                  const Section& section,
                                  IntegrationScheme scheme) {
	validateMesh(mesh);

	const int dofPerNode = determineDofPerNode(mesh);

	const int total = mesh.nodeCount() * dofPerNode;
	Eigen::MatrixXd K = Eigen::MatrixXd::Zero(total, total);

	for (std::size_t e = 0; e < mesh.element_type.size(); ++e) {
		const std::string& type = mesh.element_type[e];
		const std::vector<NodeId>& nodes = mesh.element_nodes[e];

		const std::unique_ptr<Element> element = ElementRegistry::create(type);

		// ---- 单元的空间维数必须和网格一致 ----
		if (element->spatialDim() != mesh.spatialDim()) {
			throw InputError("单元 " + std::to_string(e) + "（" + type + "）要求 "
			                 + std::to_string(element->spatialDim()) + " 维坐标，但网格是 "
			                 + std::to_string(mesh.spatialDim()) + " 维");
		}

		if (static_cast<int>(nodes.size()) != element->nodeCount()) {
			throw InputError("单元 " + std::to_string(e) + "（" + type + "）需要 "
			                 + std::to_string(element->nodeCount()) + " 个节点，实际给了 "
			                 + std::to_string(nodes.size()) + " 个");
		}

		// ---- 单元上下文 ----
		ElementContext ctx;
		ctx.node_coords = gatherNodeCoords(mesh, nodes, mesh.spatialDim(),
		                                   static_cast<int>(e));
		ctx.material    = &material;
		ctx.section     = &section;
		ctx.scheme      = scheme;

		// ---- 单元刚度阵 ----
		Eigen::MatrixXd Ke;
		element->stiffness(ctx, Ke);

		const int n = element->totalDof();
		if (Ke.rows() != n || Ke.cols() != n) {
			throw SolverError("单元 " + std::to_string(e) + "（" + type
			                  + "）返回的刚度阵尺寸 " + std::to_string(Ke.rows()) + "x"
			                  + std::to_string(Ke.cols()) + "，与声明的 "
			                  + std::to_string(n) + "x" + std::to_string(n) + " 不符");
		}

		// ---- 散列进总刚 ----
		for (std::size_t a = 0; a < static_cast<std::size_t>(n); ++a) {
			const int ia = globalDof(nodes, dofPerNode,
			                         a / static_cast<std::size_t>(dofPerNode),
			                         static_cast<int>(a) % dofPerNode);
			for (std::size_t b = 0; b < static_cast<std::size_t>(n); ++b) {
				const int ib = globalDof(nodes, dofPerNode,
				                         b / static_cast<std::size_t>(dofPerNode),
				                         static_cast<int>(b) % dofPerNode);
				K(ia, ib) += Ke(static_cast<Eigen::Index>(a),
				                static_cast<Eigen::Index>(b));
			}
		}
	}

	return K;
}

Eigen::VectorXd assembleLoadVector(const Mesh& mesh,
                                   const Material& material,
                                   const Section& section,
                                   IntegrationScheme scheme,
                                   Real transverseLoad) {
	validateMesh(mesh);

	const int dofPerNode = determineDofPerNode(mesh);
	const int total = mesh.nodeCount() * dofPerNode;

	Eigen::VectorXd F = Eigen::VectorXd::Zero(total);

	for (std::size_t e = 0; e < mesh.element_type.size(); ++e) {
		const std::string& type = mesh.element_type[e];
		const std::vector<NodeId>& nodes = mesh.element_nodes[e];

		const std::unique_ptr<Element> element = ElementRegistry::create(type);

		if (static_cast<int>(nodes.size()) != element->nodeCount()) {
			throw InputError("单元 " + std::to_string(e) + "（" + type + "）需要 "
			                 + std::to_string(element->nodeCount()) + " 个节点，实际给了 "
			                 + std::to_string(nodes.size()) + " 个");
		}

		ElementContext ctx;
		ctx.node_coords     = gatherNodeCoords(mesh, nodes, mesh.spatialDim(),
		                                       static_cast<int>(e));
		ctx.material        = &material;
		ctx.section         = &section;
		ctx.scheme          = scheme;
		ctx.transverse_load = transverseLoad;

		Eigen::VectorXd Fe;
		element->loadVector(ctx, Fe);

		const int n = element->totalDof();
		if (Fe.size() != n) {
			throw SolverError("单元 " + std::to_string(e) + "（" + type
			                  + "）返回的载荷向量长度 " + std::to_string(Fe.size())
			                  + "，与声明的 " + std::to_string(n) + " 不符");
		}

		// 散列进全局载荷向量
		for (std::size_t a = 0; a < static_cast<std::size_t>(n); ++a) {
			const int ia = globalDof(nodes, dofPerNode,
			                         a / static_cast<std::size_t>(dofPerNode),
			                         static_cast<int>(a) % dofPerNode);
			F(ia) += Fe(static_cast<Eigen::Index>(a));
		}
	}

	return F;
}

} // namespace feml
