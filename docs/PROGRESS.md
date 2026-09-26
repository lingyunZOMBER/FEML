# FEML 开发进度

> 本文档记录项目的当前状态、已定决策、踩过的坑和下一步计划。
> 每次开发告一段落时更新，便于下次直接接上，不用翻聊天记录。

**最后更新**：2026-09-26

---

## 一、当前状态

| 项目 | 状态 |
|---|---|
| 仓库 | https://github.com/lingyunZOMBER/FEML （`main` 分支） |
| 工程 | 可配置 ✅ 可编译 ✅ 可运行 ✅ 可测试 ✅ |
| 进度 | **第 1 步（工程骨架）完成** → **第 2 步（一维高斯积分）完成** → 第 3 步待开始 |
| 测试 | 8 个用例全部通过 |

**当前能跑的东西**

```
bin/feml.exe         → 打印 "FEML 0.1.0" / "Eigen 3.4.1"
feml_tests.exe       → 8 个用例通过（1 个冒烟 + 7 个高斯积分）
ctest                → 100% tests passed, 0 tests failed out of 8
```

---

## 二、快速命令参考

### 在 Visual Studio 里

| 操作 | 方式 |
|---|---|
| 配置 | **项目 → 配置缓存**（保存 `CMakeLists.txt` 后通常自动触发） |
| 彻底重来 | **项目 → 删除缓存并重新配置** |
| 编译 | **Ctrl+Shift+B** |
| 运行主程序 | 启动项选 `feml_main.exe` → **Ctrl+F5** |
| 调试测试 | 启动项选 `feml_tests.exe` → **F5** |
| 看 CMake 日志 | 视图 → 输出，来源选 **CMake** |
| 看编译日志 | 视图 → 输出，来源选 **生成** |

> 构建目录是 `out/build/x64-Debug`（VS 的默认配置）。
> 产物在 `out/build/x64-Debug/bin/` 和 `lib/`。

### 命令行

普通终端里编译需要先注入 MSVC 环境变量。项目里备了一个脚本：

```
F:\vs project\FEML\out\build\build_feml.bat
```

它做两件事：`call vcvars64.bat` 注入环境，然后调 `cmake --build`。
直接双击或在任何终端里 `cmd /c` 调用都可以。

跑测试（不需要 MSVC 环境）：

```
cd /d "F:\vs project\FEML"
"F:\visual studio\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe" ^
    --test-dir out\build\x64-Debug --output-on-failure
```

直接跑测试程序（支持 Catch2 的过滤器语法）：

```
out\build\x64-Debug\bin\feml_tests.exe              :: 全部
out\build\x64-Debug\bin\feml_tests.exe "[beam2]"    :: 只跑某个标签
out\build\x64-Debug\bin\feml_tests.exe -s           :: 打印每个通过的断言
out\build\x64-Debug\bin\feml_tests.exe --list-tests :: 列出所有用例名
```

---

## 三、已完成的工作

### 提交历史

```
b758c94  docs: 添加开发进度文档，记录已完成工作、决策与踩坑经验
62acc2c  test: 接入 Catch2 与 CTest，添加版本号冒烟测试
9eaeba6  feat: 搭建 CMake 工程骨架，接通 Eigen 与 Catch2
9ae335c  Update .gitignore
3e06923  Initial commit
```

### 文件清单

```
CMakeLists.txt                          工程入口：语言标准、编译选项、子目录
cmake/Dependencies.cmake                依赖管理（Eigen + Catch2，FetchContent）

include/feml/core/types.hpp             地基：Real / Index / 常量
include/feml/core/exceptions.hpp        异常层次：FemlError / InputError / ...
include/feml/core/version.hpp           版本号
include/feml/element/integration_rule.hpp  高斯积分点、积分规则、积分方案

src/core/version.cpp
src/element/integration_rule.cpp        高斯点与权重数值、方案关键字解析
src/CMakeLists.txt                      feml 静态库目标

apps/feml_main.cpp                      主程序
apps/CMakeLists.txt

tests/test_smoke.cpp                    版本号冒烟测试
tests/test_integration_rule.cpp         高斯积分 7 个用例
tests/CMakeLists.txt                    测试目标 + CTest 注册

docs/PROGRESS.md                        本文档
.gitignore / .gitattributes / LICENSE / README.md / 第2次作业.doc
```

### 目录结构约定

```
FEML/
├─ CMakeLists.txt          工程入口
├─ cmake/                  构建辅助脚本
├─ include/feml/           公开头文件（include 根 = 命名空间路径）
│  ├─ core/                基础类型、异常、版本
│  └─ element/             单元相关
├─ src/                    对应实现（同名同路径）
│  ├─ core/
│  └─ element/
├─ apps/                   可执行程序
├─ tests/                  单元测试
├─ docs/                   文档
└─ out/ 或 build/          构建产物（已 gitignore）
```

**约定**：`include/feml/core/version.hpp` ↔ `src/core/version.cpp` 同名同路径，
代码里写 `#include "feml/core/version.hpp"`（路径相对 `include/` 根）。

---

## 四、技术决策

| 项目 | 选择 | 理由 |
|---|---|---|
| 语言标准 | **C++20** | 需要 `std::format`、`inline constexpr`、`enum class`、`std::numbers` |
| 构建系统 | **CMake ≥3.25 + Ninja**（VS 内置） | 跨平台、VS 原生支持、随时可生成 .sln |
| 线性代数 | **Eigen 3.4.1** | 保留 `LinearSolver` 抽象层，日后自己实现可对照 |
| 测试框架 | **Catch2 v3.9.1 + CTest** | 单头文件；`catch_discover_tests` 让每个用例独立可见 |
| 依赖获取 | **FetchContent + 锁定 URL/SHA256** | 版本写在代码里，任何人 clone 都能复现 |
| 架构分层 | apps → analysis → assembly → element → model → linalg → core | 依赖严格单向，上层依赖下层 |

**锁定的依赖版本**

```cmake
# Eigen 3.4.1
URL      https://gitlab.com/libeigen/eigen/-/archive/3.4.1/eigen-3.4.1.tar.gz
URL_HASH SHA256=b93c667d1b69265cdb4d9f30ec21f8facbbe8b307cf34c0b9942834c6d4fdbe2

# Catch2 v3.9.1
URL      https://github.com/catchorg/Catch2/archive/refs/tags/v3.9.1.tar.gz
URL_HASH SHA256=a215c2a723bd7483efd236dc86066842a389cb4e344c61119c978acdf24d39be
```

**贯穿全局的四条设计**（方向已定，尚未落地）

1. `Element` 多态基类 + 注册表 —— 新增单元只写一个文件，不改已有代码
2. 积分方案是**运行时参数**，不是编译期分支（作业要对比完全/减缩积分）
3. `DofManager` 管"节点分量 → 方程号" —— 以后扩到 2D/3D 单元时核心不动
4. `LinearSolver` 抽象接口 —— 求解器可替换

**编码纪律**

- **代码标识符用英文，注释和文档用中文**
  （测试名、标签、函数名、变量名 → 英文；注释、文档、提交信息正文 → 中文）
- **头文件里禁止 `using namespace std;`**，`.cpp` 里也统一不用，一律写 `std::` 前缀

---

## 五、踩过的坑（经验记录）

### git

- `.gitignore` **只对未跟踪文件生效**。已提交的文件要先 `git rm --cached`
- **不能穿透被排除的目录**：写了 `build/` 之后，`!build/keep.txt` 是**无效**的
- 不带 `/` 的模式在**任意层级**匹配（`build/` 会匹配任何名为 `build` 的目录）
- 调试利器：`git check-ignore -v <路径>` —— 直接告诉你是哪一行规则命中的
- `git commit` 只提交**暂存区**，不先 `git add` 就是空提交

### CMake

**顺序有语义，不是普通配置文件**：

| 指令 | 约束 |
|---|---|
| `cmake_minimum_required` | 必须第一行 |
| `project()` | 必须在任何 target 之前 |
| `set(CMAKE_CXX_STANDARD ...)` | 必须在建 target 之前 |
| `set(某个库的选项)` | 必须在 `FetchContent_MakeAvailable` 之前 |
| `include(cmake/Dependencies.cmake)` | 必须在链接它的 target 之前 |
| `enable_testing()` | 必须在 `add_subdirectory(tests)` 之前 |

> **核心原理**：CMake 变量在被读取的那一刻取值，不是"设了就全局生效"。

**FetchContent**：

- `FetchContent_Declare` **只声明不下载**；`FetchContent_MakeAvailable` 才真正下载
- 变量名是**小写**：`eigen_SOURCE_DIR`，不是 `Eigen_SOURCE_DIR`
- 用 `SOURCE_SUBDIR` 指向不存在的目录，可跳过该库自己的 CMake 逻辑
- 下载失败可用 `-DFETCHCONTENT_SOURCE_DIR_<NAME>=<本地目录>` 走离线源码

**target 属性**：

- **ALIAS target 不能改属性**。`Catch2::Catch2` 是别名，真实目标是 `Catch2`；
  用 `get_target_property(... ALIASED_TARGET)` 解析
- `PUBLIC` / `PRIVATE` 决定的是**可见性**：对纯头文件库（Eigen）不产生链接动作，
  真正作用是**传播包含路径**。写成 `PRIVATE`，`apps` 就找不到 `<Eigen/Core>`
- 第三方库的头文件目录要标 `SYSTEM`，否则 `/W4` 下会被它的警告淹没
- 编译选项用 **INTERFACE 目标**而不是全局 `add_compile_options()`
- 源文件列表**显式列出，不要用 `file(GLOB)`**（新增文件不会自动触发重新配置）

> `D9025: 正在重写"/W3"(用"/W4")` 是命令行标志提示，不是代码警告。
> CMake 默认给 MSVC 加 `/W3`，被我们的 `/W4` 覆盖，属于正常现象。

### C++

- **头文件里不能写 `using namespace std;`**。头文件内容会被复制到每个
  `#include` 它的地方，于是这行会传播到全项目。后果：`feml::string`、
  `feml::count` 全都成了 `std::` 的名字，重载解析变得不可预测，
  且升级标准库可能突然撞名编译失败
- **构造函数名必须与类名相同**。写成基类名会导致编译错误
  （C++ 会把那个名字当成"返回类型"，而构造函数不允许有返回类型）
- 头文件必须**自包含**：用到什么就包含什么，不能依赖"别人先包含了"
- **包含你所使用的东西**：`.cpp` 里直接用了 `std::string` 就显式 `#include <string>`，
  不要靠间接包含
- 头文件里定义变量必须加 `inline`（C++17 起），否则链接时"多重定义"
- 单语句 `if` 建议也加大括号——维护期加第二行时很容易忘记补

### Windows 编码

- **中文不能用作测试名 / 标签**。Windows 的 `main(argc, argv)` 收到的是
  ANSI 代码页（GBK）编码，而 `/utf-8` 编译的程序内字符串是 UTF-8 字节
  → 字节比较失败 → `No test cases matched`
- **中文输入法会导致全角标点混入代码**。全角 `，` 是 3 字节（`EF BC 8C`），
  半角 `,` 是 1 字节（`2C`），视觉上只宽一点点，但会导致编译错误，
  且报错信息指向的位置"看着没问题"
- **检测方法**（剥掉注释和字符串后再查）：

  ```bash
  sed 's|//.*||' 文件 | sed 's|"[^"]*"||g' | grep -nP '[，；：（）！？【】]'
  ```

- 全角标点在**注释和字符串里**是安全的，只在**代码位置**才是错误

### Visual Studio 工具问题（均非项目问题）

- **"IntelliSense 信息可能过时"** 是正常提示，重新生成即可。
  **红色波浪线 ≠ 编译错误**，最终裁判永远是编译器和链接器
- **新增的文件在加入 CMakeLists 后，IntelliSense 仍可能报错**
  （如"未定义标识符 Real"）。原因：IntelliSense 的包含路径来自 CMake 代码模型，
  而代码模型只为"属于某个构建目标的文件"生成。**保存 + 重新配置**即可解决
- **测试资源管理器为空**：VS 的 `ExternalBuildProjectDiscoverer` 组件加载失败，
  属于 VS 安装缺陷。但 `out/build/x64-Debug/FEML.json` 里有正确的测试描述，
  证明 CMake 侧一切正常。**替代方案**：启动项设为 `feml_tests.exe` 按 F5
- **多次出现"写完但磁盘是 0 字节"**：忘记 `Ctrl+S`。
  验证方法：`wc -c 文件路径`

### 测试方法

- **不只验证"该对的对了"，还要验证"该错的地方确实错了"**
  - 例：高斯积分的测试是**配套的两条**——「n 点精确积分到 2n−1 次」
    和「n 点不能精确积分 2n 次」
  - 只测前者的话，一个"意外地对"的错误实现也能通过
  - 后面写梁单元时同理：既要验证"减缩积分解精确"，也要验证"完全积分确实锁死"
- Catch2 的浮点比较用匹配器，不要手写 `abs(a-b) < eps`：

  ```cpp
  #include <catch2/matchers/catch_matchers_floating_point.hpp>
  using Catch::Matchers::WithinAbs;
  REQUIRE_THAT(value, WithinAbs(expected, 1e-13));
  ```

- `INFO(...)` 给断言加失败上下文（循环里定位是哪个 n / 哪个 p 挂了）
- `REQUIRE` 失败中止当前用例；`CHECK` 失败继续执行

---

## 六、下一步：第 3 步 · 单元抽象与注册表

### 为什么这是关键一步

这是整个架构的**核心扩展点**。做完之后，第 4 步写 2 节点 Timoshenko 梁单元时，
就只需要**新增一个文件**，不用改任何已有代码——这是本项目相对传统教学程序
最重要的结构性改进。

### 待写文件

| # | 文件 | 内容 |
|---|---|---|
| 1 | `include/feml/model/material.hpp` | 材料（E、ν） |
| 2 | `include/feml/model/section.hpp` | 截面（A、I、剪切修正因子 k） |
| 3 | `include/feml/element/element.hpp` | **`Element` 抽象基类** + `ElementContext` + `ElementResult` |
| 4 | `include/feml/element/registry.hpp` + `src/element/registry.cpp` | 单元注册表 |
| 5 | `tests/test_element_interface.cpp` | 用一个极简"杆单元"验证接口可用 |

### 核心接口设计（草案）

```cpp
class Element {
public:
    virtual ~Element() = default;

    // 基本信息
    virtual std::string_view name() const = 0;        // "BEAM2"
    virtual int nodeCount()  const = 0;               // 2
    virtual int dofPerNode() const = 0;               // 2（w, θ）
    virtual int spatialDim() const = 0;               // 1 梁 / 2 平面 / 3 实体
    virtual std::vector<std::string> dofNames() const = 0;

    int totalDof() const { return nodeCount() * dofPerNode(); }

    // 核心计算
    virtual void stiffness(const ElementContext& ctx, Eigen::MatrixXd& Ke) const = 0;
    virtual void loadVector(const ElementContext& ctx, Eigen::VectorXd& Fe) const;
    virtual void recover(const ElementContext& ctx, ElementResult& out) const;
};
```

`ElementContext` 打包单元需要的全部输入：节点坐标、材料指针、截面指针、
积分方案。**单元只通过它接触外界，不直接访问 Mesh** —— 这样单元可以脱离
整个程序单独测试（给定坐标和材料就能算刚度阵）。

### 一个必须注意的坑

**静态库的注册表失效问题**：如果用"静态初始化 + 宏注册"的方式，
链接器可能因为"没人引用"把整个 `.o` 文件丢掉，注册会**静默失效**——
不报错，只是单元找不到。

稳妥做法：先写一个显式的 `registerBuiltinElements()` 函数在 `main` 里调用。
这样虽然是"一个中心注册点"，但相比传统教学程序要改 10 个文件，已经是巨大改进。

### 第 3 步的核心理论

单元刚度矩阵的一般形式，以及"需要几阶积分"的推理链条：

```
形函数次数 → 应变分量次数 → B 矩阵次数 → B^T D B 次数 → 所需积分点数
```

**注意 $B^T D B$ 里 $B$ 被平方了，所以次数翻倍**——这是决定积分阶次的关键。

2 节点 Timoshenko 梁的结论：

| 项 | B 的次数 | $B^T D B$ 的次数 | 精确所需点数 |
|---|---|---|---|
| 弯曲 κ = dθ/dx | 0（常数） | 0 | 1 |
| 剪切 γ = dw/dx − θ | 1（线性） | 2 | 2 |

---

## 七、后续路线图

```
✅ 1. 工程骨架（CMake + Eigen + Catch2 + 测试框架）
✅ 2. 一维高斯积分规则
🔄 3. 单元抽象 + 注册表（Element 基类、ElementContext、ElementRegistry）
   4. 2 节点 Timoshenko 梁单元          ← 作业核心
   5. 装配 + 约束施加 + 线性求解
   6. 3 节点 Timoshenko 梁单元
   7. 卡片式输入文件 + 结果输出
   8. 细长比参数扫描 + CSV 输出
```

### 关键力学公式（第 4 步用）

2 节点 Timoshenko 梁单元，自由度 (w₁, θ₁, w₂, θ₂)：

$$\Phi = \frac{12EI}{kGA L^2} = \frac{E\,h^2}{k\,G\,L^2} \quad (\text{矩形截面 } k = 5/6)$$

$$K = \frac{EI}{L^3(1+\Phi)}
\begin{bmatrix}
12 & 6L & -12 & 6L \\
6L & (4+\Phi)L^2 & -6L & (2-\Phi)L^2 \\
-12 & -6L & 12 & -6L \\
6L & (2-\Phi)L^2 & -6L & (4+\Phi)L^2
\end{bmatrix}$$

Φ → 0 时退化为 Euler-Bernoulli 梁。

**必做检查**：K 的零特征值个数 == 刚体模态数（2 节点单元为 2）。
这一条能挡住绝大部分零能模态 bug。

**为什么完全积分会锁死**：4 个自由度减去 2 个刚体模态，K 的秩只需要 2。
1 点积分时弯曲贡献秩 1、剪切贡献秩 1，恰好是 2；而 2 点积分让剪切贡献变成秩 2，
总秩 3 —— **多出来的那一个约束是虚假的**，它强行压制弯曲变形，这就是剪切锁死的本质。

### 3 节点梁单元的预判（第 6 步）

| 项 | B 的次数 | $B^T D B$ 的次数 | 精确所需点数 |
|---|---|---|---|
| 弯曲 κ = dθ/dx | 1（线性） | 2 | 2 |
| 剪切 γ = dw/dx − θ | 2（二次） | 4 | 3 |

所以 **3 节点单元需要最多 3 点积分**——这就是 `GaussRule1D` 支持到 4 点的原因。
秩检查：6 自由度 − 2 刚体 = 需秩 4；2 点弯曲秩 2 + 2 点剪切秩 2 = 4 ✓ 恰好。

---

## 八、挂起的事项

1. **「细长比」的定义需要跟老师确认**
   - 若指 L/h，趋近 0 是粗短梁（剪切变形主导，差异来自剪切项）
   - 若指 h/L，趋近 0 是细长梁（Φ→0，正是剪切锁死发作处）
   - 两者讨论的物理现象**完全不同**
   - **对策**：程序把 h/L 做成扫描参数、覆盖两个方向，报告里两个极限都讨论

2. **交作业前考虑把依赖 vendor 进仓库**
   - 当前 FetchContent 首次配置需要联网，国内访问 GitHub 不稳定（已遇到一次瞬时失败）
   - 老师/助教那边若无法联网将无法构建
   - 方案：`git clone --depth 1` 到 `third_party/`，去掉 `.git` 后纳入仓库

3. **VS 测试资源管理器**（可选，非阻塞）
   - 可用 VS Installer → 修改 → 修复 尝试修复
   - 或用 F5 调试 `feml_tests.exe` 替代

4. **`CMakePresets.json` 尚未创建**
   - 现在 VS 用的是自己的默认配置（`out/build/x64-Debug`）
   - 好处是零配置可用；代价是不能一键切 Release、构建目录不统一
   - 等需要跑细长比扫描（性能敏感）时再补，不影响开发
