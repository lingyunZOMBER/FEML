#include "feml/element/register_builtin.hpp"

#include "feml/element/beam2_timoshenko.hpp"
#include "feml/element/registry.hpp"

#include <memory>

namespace feml {

/// 所有内置单元的登记点。
///
/// 新增一种单元时，只要在这里加一行：
///     ElementRegistry::add("BEAM3", [] { return std::make_unique<Beam3Timoshenko>(); });
///
/// 名字必须与该单元 name() 的返回值一致
void registerBuiltinElements() {
	ElementRegistry::add("BEAM2", [] {
		return std::make_unique<Beam2Timoshenko>();
	});
}

} // namespace feml
