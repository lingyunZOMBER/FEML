#pragma once

#include "feml/core/types.hpp"

#include <Eigen/Core>

#include <string>
#include <vector>

namespace feml {

/// 网格：节点坐标 + 单元连接。

struct Mesh {
	/// 节点坐标，每行一个节点，列数为空间维数。
	/// 梁：n×1；平面/板：n×2；实体：n×3。
	Eigen::MatrixXd node_coords;

	/// 单元类型名（如 "BEAM2"），与 element_nodes 一一对应
	std::vector<std::string> element_type;

	/// 每个单元的节点连接，与 element_type 一一对应
	std::vector<std::vector<NodeId>> element_nodes;

	int nodeCount() const {
		return static_cast<int>(node_coords.rows());
	}
	int elementCount() const {
		return static_cast<int>(element_type.size());
	}
	int spatialDim() const {
		return static_cast<int>(node_coords.cols());
	}
};

} // namespace feml
