#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace feml {
	// 全局实数类型
	using Real = double;

	//索引类型
	using Index = std::int32_t;

	//节点编号，单元编号
	using NodeId = Index;
	using ElementId = Index;

	//自由度分量编号
	using DofComponent = Index;

	//表示【无效】的索引值
	inline constexpr Index KInvalidIndex = -1;

	//单个节点可能拥有的最大自由度
	inline constexpr int KMaxDofPerNode = 6;

	using StringList = std::vector<std::string>;
} //namespace feml
