#include "feml/element/register_builtin.hpp"

namespace {

/// 测试可执行文件启动时，把内置单元注册一次。
///
/// 【为什么放在这里，而不是让每个测试文件各自调用】
/// `catch_discover_tests` 让每个 TEST_CASE 在【独立进程】里运行
/// （每个 CTest 测试项就是一次 feml_tests.exe <过滤器> 调用）。
/// 所以每个进程都必须自己完成注册 —— 靠"每个测试文件记得调一次"
/// 迟早会漏，而且漏了的后果是运行时错误，要跑测试才发现。
///
/// 【为什么这里可以用静态注册，而生产代码不行】
/// 生产代码里 feml 是【静态库】，链接器只会挑选"被引用到"的目标文件，
/// 靠静态初始化的自动注册可能被整个丢掉（详见 register_builtin.hpp）。
/// 但测试可执行文件是把所有 .cpp 直接编译链接进来的，不存在这个问题。
///
/// registerBuiltinElements() 是幂等的，所以即使某个测试文件也显式调用了，
/// 也不会冲突。
const bool g_elementsRegistered = [] {
	feml::registerBuiltinElements();
	return true;
}();

} // namespace
