#pragma once

#include "feml/core/types.hpp"
#include "feml/element/integration_rule.hpp"
#include "feml/model/mesh.hpp"

#include <Eigen/Core>

namespace feml {

struct Material;
struct Section;

/// 组装总刚 K。
///
/// 【矩阵的排列方式】
/// 行列按 (节点, 分量) 展平排列：第 (i * dofPerNode + c) 行对应节点 i 的第 c 个自由度。
/// 这个顺序与 DofManager 的展平方式一致，所以两者可以直接换算。
/// @throws InputError 网格数据不自洽（节点越界、数组长度不符、自由度数不一致等）
/// @throws SolverError 某个单元返回的刚度阵尺寸与其声明的自由度数不符
Eigen::MatrixXd assembleStiffness(const Mesh& mesh,
                                  const Material& material,
                                  const Section& section,
                                  IntegrationScheme scheme);

/// 组装载荷向量 F（尺寸 = 节点数 × 每节点自由度数）。
///
/// 逐单元调用 Element::loadVector()，把单元的一致节点载荷散列到全局向量中。
/// 载荷大小由 transverseLoad 给出（板单元为横向均布载荷 q [N/m²]；
/// 梁单元不使用该参数，其载荷向量为零）。
///
/// @throws InputError 网格数据不自洽
Eigen::VectorXd assembleLoadVector(const Mesh& mesh,
                                   const Material& material,
                                   const Section& section,
                                   IntegrationScheme scheme,
                                   Real transverseLoad);

} // namespace feml
