#include "feml/element/register_builtin.hpp"

#include "feml/element/beam2_timoshenko.hpp"
#include "feml/element/beam3_timoshenko.hpp"
#include "feml/element/registry.hpp"

#include <memory>

namespace feml {

/// 所有内置单元的登记点。
///
/// 新增一种单元时，只要在这里加两行：一个 include 加一条 add。
///
/// 名字必须与该单元 name() 的返回值一致
void registerBuiltinElements() {
	static const bool registered = [] {
		ElementRegistry::add("BEAM2", [] {
			return std::make_unique<Beam2Timoshenko>();
		});

		ElementRegistry::add("BEAM3", [] {
			return std::make_unique<Beam3Timoshenko>();
		});

		return true;
	}();

	(void)registered;   // 只是为了触发初始化，值本身不用
}

} // namespace feml
