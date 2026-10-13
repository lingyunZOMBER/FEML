# 踩坑记录

> 开发过程中遇到并解决的问题，按领域分类。
> 每条都尽量写清**现象 → 原因 → 做法**，而不只是结论。
>
> 项目状态与路线图见 [PROGRESS.md](PROGRESS.md)。

---

## git

**`.gitignore` 只对未跟踪文件生效**
已经 commit 过的文件，事后加进 `.gitignore` 也没用。必须先 `git rm --cached <file>`
把它从索引里摘掉（工作区的文件本身保留）。

**无法穿透被排除的目录**
写 `build/` 之后，git 根本不会进入这个目录，所以 `!build/keep.txt` 是**无效**的。
要保留目录内的个别文件，必须写成 `build/*` 加 `!build/keep.txt`。
（注意：`*.OUT` 这类只排除**文件**的模式不受此限制，`!examples/**/ref/*.OUT` 是有效的。）

**不带 `/` 的模式在任意层级匹配**
`build/` 会匹配任何名为 `build` 的目录，不只是根目录那个。想锚定根目录要写 `/build/`。

**`git commit` 只提交暂存区**
不先 `git add` 就是空提交。暂存区的意义是让你能"只提交这次想提交的部分"。

**诊断利器：`git check-ignore -v <路径>`**
直接告诉你是**哪一行规则**命中的。遇到"为什么被忽略/没被忽略"，第一条命令就用它。

**`git status` 显示 ` M` 但 `git diff` 是空的**
说明只是 stat 缓存失效，内容没变（常见于 `core.autocrlf=true` + `text=auto` 的组合）。
用 `git status --porcelain=v2 <文件>` 能看到两个哈希——相同即内容一致。
清除方式是 `git add <文件>`（重算哈希、只更新 stat，不产生内容变更）。

---

## CMake

### 顺序有语义

CMake 不是普通配置文件，**写的位置会改变含义**：

| 指令 | 约束 |
|---|---|
| `cmake_minimum_required` | 必须第一行 |
| `project()` | 必须在任何 target 之前 |
| `set(CMAKE_CXX_STANDARD ...)` | 必须在建 target 之前 |
| `set(某个库的选项)` | 必须在 `FetchContent_MakeAvailable` 之前 |
| `include(cmake/xxx.cmake)` | 必须在链接它的 target 之前 |
| `enable_testing()` | 必须在 `add_subdirectory(tests)` 之前 |

> **核心原理**：CMake 变量在被读取的那一刻取值，不是"设了就全局生效"。

### 依赖管理

- `FetchContent_Declare` **只声明不下载**；`FetchContent_MakeAvailable` 才真正下载并加入构建
- 变量名是**小写**：`eigen_SOURCE_DIR`，不是 `Eigen_SOURCE_DIR`
  （写错得到空值，报错信息完全指不到真正原因）
- 用 `SOURCE_SUBDIR` 指向不存在的目录，可跳过该库自带的 CMake 逻辑
  （Eigen 会去找 BLAS、生成 pkg-config、构建自测，全都不需要）
- 下载失败可用 `-DFETCHCONTENT_SOURCE_DIR_<NAME>=<本地目录>` 走离线源码

### target 与属性

- **ALIAS target 不能改属性**。`Catch2::Catch2` 是别名，真实目标是 `Catch2`；
  用 `get_target_property(... ALIASED_TARGET)` 解析出真实目标
- `PUBLIC` / `PRIVATE` 决定的是**可见性**。对纯头文件库（Eigen）不产生链接动作，
  真正作用是**传播包含路径**；写成 `PRIVATE`，使用者就找不到头文件
- 第三方库的头文件目录要标 `SYSTEM`，否则 `/W4` 下会被它的警告淹没
- 编译选项用 **INTERFACE 目标**而不是全局 `add_compile_options()`，
  否则会污染第三方库的警告输出
- 源文件列表**显式列出，不要用 `file(GLOB)`**（新增文件不会自动触发重新配置）

### 容易犯的错

- **`add_library` 同一目标名写了两次** → CMake 直接报错 "another target with the
  same name already exists"。改 `CMakeLists.txt` 是**修改**不是**追加**——加源文件时
  要编辑已有的源文件列表，不要在文件末尾新写一个 `add_library` 块
- `add_subdirectory(X)` / `include(X)` 都要求 `X` **真实存在**，否则配置失败

### 无害的警告

```
cl: 命令行 warning D9025 :正在重写"/W3"(用"/W4")
```
这是命令行标志提示，不是代码警告。CMake 默认给 MSVC 加 `/W3`，被我们的 `/W4` 覆盖。
它反而证明 `/W4` 生效了。

---

## C++

**头文件里不能写 `using namespace std;`**
头文件的内容会被复制到每个 `#include` 它的地方，于是这行会传播到全项目。
后果：`feml::string`、`feml::count` 全都成了 `std::` 的名字，重载解析变得不可预测，
且升级标准库时可能突然撞名编译失败。**一律写 `std::` 前缀。**

**构造函数名必须与类名相同**
写成基类名会导致编译错误（C++ 会把那个名字当成"返回类型"，而构造函数不允许有返回类型）。

**头文件必须自包含**
用到什么就包含什么，不能依赖"别人已经包含了"。
违反时的症状很迷惑：单独包含报错，但夹在别的头文件后面就正常。

**包含你所使用的东西**
`.cpp` 里直接用了 `std::find` 就显式 `#include <algorithm>`，用了 `std::string` 就
`#include <string>`。靠间接包含能编过，但那个头文件一旦不再包含它，这里就会莫名失败。

**头文件里定义变量必须加 `inline`**（C++17 起）
否则每个包含它的翻译单元各定义一份 → 链接时"多重定义"错误。

**有虚函数的基类必须有虚析构函数**
通过基类指针 `delete` 派生类对象时，若析构函数非虚，派生类的成员不会析构——未定义行为。
而且往往不崩溃，只是悄悄泄漏，所以容易漏掉。

**单语句 `if` 也建议加大括号**
维护期想加第二行语句时很容易忘记补括号，导致逻辑错误。

**函数内静态变量（Meyers singleton）优于文件级全局变量**
C++ 不保证不同翻译单元里全局变量的初始化顺序。如果注册代码先于注册表构造执行，
就会在未初始化的容器上插入元素。函数内静态变量在**首次调用时**才初始化，
且 C++11 起保证线程安全。

---

## Windows 编码

**中文不能用作测试名 / 标签**
Windows 的 `main(argc, argv)` 收到的是 **ANSI 代码页（GBK）** 编码，而 `/utf-8` 编译的
程序内字符串是 **UTF-8 字节** → 字节比较失败 → Catch2 报 `No test cases matched`。

**纪律：代码标识符用英文，注释和文档用中文。**
测试名、标签、函数名、变量名会流经命令行参数、日志文件、XML 报告、CI 系统——
全是编码脆弱的地方。注释只存在于源码里，中文完全安全。

**中文输入法会导致全角标点混入代码**

| 字符 | 编码 | 字节数 |
|---|---|---|
| `,` 半角 | `2C` | 1 |
| `，` 全角 | `EF BC 8C` | 3 |

视觉上只宽一点点，但会导致编译错误，而且报错位置"看着没问题"。
**报错指向一行明明没问题的代码时，第一反应就去看标点是不是全角。**

检测方法（剥掉注释和字符串后检查）：

```bash
# 剥掉 // 注释，再剥掉字符串，再查全角标点
sed 's|//.*||' 文件 | sed 's|"[^"]*"||g' | grep -nP '[，；：（）！？【】]'
```

> **这个检查有假阳性**：`sed 's|"[^"]*"||g'` 处理不了**转义引号** `\"`。
> 例如 `"\"，可用："` 会被错误配对，导致字符串内的全角逗号被报出来。
> **看到可疑结果要人工确认上下文**——全角标点在注释和字符串里是安全的，
> 只在代码位置才是错误。

---

## Visual Studio

**"IntelliSense 信息可能过时"是正常提示**
编辑 `CMakeLists.txt` 后 VS 会提示，点提示里的「生成」即可刷新。

**红色波浪线 ≠ 编译错误**
IntelliSense 和真正的编译是两条独立链路。常见情况：
- 波浪线报错但能编过（IntelliSense 信息过时）
- 波浪线干净但编不过（它不做模板实例化和链接检查）

**最终裁判永远是编译器和链接器。**

**新增的文件在加入 CMakeLists 后，IntelliSense 仍可能报"未定义标识符"**
原因：IntelliSense 的包含路径来自 CMake 的**代码模型**，而代码模型只为"属于某个构建
目标的文件"生成。新文件在 CMake 重新配置之前不属于任何目标。
**解决：保存 + 项目 → 配置缓存。**

**测试资源管理器为空**
日志里会看到：
```
测试发现器"ExternalBuildProjectDiscoverer"加载测试时发生异常:
未能加载文件或程序集"Microsoft.VisualStudio.Workspace.ExternalBuildFramework.Impl..."
```
这是 VS 安装缺陷（组件依赖缺失），**不是项目问题**。证据：`out/build/x64-Debug/FEML.json`
里有完全正确的测试描述。

**替代方案**：启动项选 `feml_tests.exe` → **Ctrl+F5**（跑全部）或 **F5**（带调试器，
断点、单步、监视变量全部可用）。传命令行参数（如只要 `[element]` 标签）用
**项目 → 调试和启动设置**。

**文件"写完了"但磁盘是 0 字节**
忘记 `Ctrl+S`。VS 标签页上有小圆点表示未保存。
验证方法：`wc -c 文件路径`。这个坑在开发中出现过多次。

---

## 测试方法

**不只验证"该对的对了"，还要验证"该错的地方确实错了"**

例：高斯积分的测试是**配套的两条**——
「n 点精确积分到 2n−1 次」和「n 点不能精确积分 2n 次」。

只测前者的话，一个"意外地对"的错误实现也能通过。第二条把规则钉死在
"恰好是 2n−1 阶精度"上。

后面写梁单元时同理：既要验证"减缩积分解精确"，也要验证"完全积分确实锁死"。

**优先写不依赖具体数值的物理性质检查**

| 性质 | 依据 | 能挡住什么 |
|---|---|---|
| K 对称 | 刚度阵源于能量泛函的二阶导数 | 下标错位、符号错 |
| 刚体模态零能 | 整体平移/转动不产生节点力 | 大部分实现错误 |
| 零特征值个数 == 刚体模态数 | 刚度阵的零空间 | 零能模态/积分不足 |

**Catch2 用法要点**

```cpp
#include <catch2/catch_test_macros.hpp>                          // TEST_CASE / REQUIRE
#include <catch2/matchers/catch_matchers_floating_point.hpp>      // 浮点匹配器

using Catch::Matchers::WithinAbs;
REQUIRE_THAT(value, WithinAbs(expected, 1e-13));   // 比手写 abs(a-b) < eps 好
```

- **Catch2 v3 拆分了头文件**。网上教程里的 `<catch2/catch.hpp>` 是 **v2 的写法，v3 里不存在**
- `REQUIRE` 失败**中止**当前用例；`CHECK` 失败**继续**执行
- `INFO(...)` 给断言加失败上下文（循环里定位是哪个 `n`、哪个 `p` 挂了）
- 测试名用英文；标签（`[element]`）也是英文

**`catch_discover_tests` 让每个用例在独立进程运行**
每个 CTest 测试项就是一次 `feml_tests.exe <过滤器>` 调用。
所以全局单例（如 `ElementRegistry`）在测试之间**不会互相污染**——
测试里注册的假单元不会泄漏到别的测试或生产代码。
