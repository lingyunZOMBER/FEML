# FEML 开发进度

> 本文档记录项目的当前状态、已定决策、踩过的坑和下一步计划。
> 每次开发告一段落时更新，便于下次直接接上，不用翻聊天记录。

**最后更新**：2026-09-22

---

## 一、当前状态

| 项目 | 状态 |
|---|---|
| 仓库 | https://github.com/lingyunZOMBER/FEML （`main` 分支） |
| 最新提交 | `62acc2c` test: 接入 Catch2 与 CTest，添加版本号冒烟测试 |
| 工程 | 可配置 ✅ 可编译 ✅ 可运行 ✅ 可测试 ✅ |
| 进度 | 第 1 步（工程骨架）完成 → 第 2 步（一维高斯积分）待开始 |

**当前能跑的东西**

```
bin/feml.exe         → 打印 "FEML 0.1.0" / "Eigen 3.4.1"
bin/feml_tests.exe   → 1 个测试用例通过（2 个断言）
ctest                → 1/1 Passed
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

### 命令行（需在「Developer Command Prompt for VS 2026」中）

```
cd /d "F:\vs project\FEML"

:: 编译
cmake --build out\build\x64-Debug

:: 跑全部测试
ctest --test-dir out\build\x64-Debug --output-on-failure

:: 直接跑测试程序
out\build\x64-Debug\bin\feml_tests.exe            :: 全部
out\build\x64-Debug\bin\feml_tests.exe "[beam2]"  :: 只跑某个标签
out\build\x64-Debug\bin\feml_tests.exe -s         :: 打印每个通过的断言
out\build\x64-Debug\bin\feml_tests.exe --list-tests  :: 列出所有用例
```

---

## 三、已完成的工作

### 提交历史

```
62acc2c  test: 接入 Catch2 与 CTest，添加版本号冒烟测试
9eaeba6  feat: 搭建 CMake 工程骨架，接通 Eigen 与 Catch2
9ae335c  Update .gitignore
3e06923  Initial commit
```

### 文件清单

```
CMakeLists.txt                    工程入口：语言标准、编译选项、子目录
cmake/Dependencies.cmake          依赖管理（Eigen + Catch2，FetchContent）
include/feml/core/version.hpp     公开头文件示例
src/core/version.cpp              实现
src/CMakeLists.txt                feml 静态库目标
apps/feml_main.cpp                主程序
apps/CMakeLists.txt               可执行目标
tests/test_smoke.cpp              冒烟测试
tests/CMakeLists.txt              测试目标 + CTest 注册
.gitignore                        忽略构建产物、VS 状态、CMake 中间文件
.gitattributes / LICENSE / README.md / 第2次作业.doc
```

### 目录结构约定

```
FEML/
├─ CMakeLists.txt          工程入口
├─ cmake/                  构建辅助脚本
├─ include/feml/           公开头文件（include 根 = 命名空间路径）
│  └─ <模块>/
├─ src/                    对应实现（同名同路径）
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
| 语言标准 | **C++20** | 需要 `std::format`、`inline constexpr`、`enum class` |
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

---

## 五、踩过的坑（经验记录）

### git

- `.gitignore` **只对未跟踪文件生效**。已提交的文件要先 `git rm --cached`
- **不能穿透被排除的目录**：写了 `build/` 之后，`!build/keep.txt` 是**无效**的
- 不带 `/` 的模式在**任意层级**匹配（`build/` 会匹配任何名为 `build` 的目录）
- 调试利器：`git check-ignore -v <路径>` —— 直接告诉你是哪一行规则命中的
- `git commit` 只提交**暂存区**，不先 `git add` 就是空提交
- `git status --porcelain=v2` 能看出"stat 缓存失效但内容相同"这种情况（两个哈希相同）

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
  （写错得到空值，报错完全指不到真正原因）
- 用 `SOURCE_SUBDIR` 指向不存在的目录，可跳过该库自己的 CMake 逻辑
- 下载失败可用 `-DFETCHCONTENT_SOURCE_DIR_<NAME>=<本地目录>` 走离线源码

**target 属性**：

- **ALIAS target 不能改属性**。`Catch2::Catch2` 是别名，真实目标是 `Catch2`；
  用 `get_target_property(... ALIASED_TARGET)` 解析
- `PUBLIC` / `PRIVATE` 决定的是**可见性**：对纯头文件库（Eigen）不产生链接动作，
  真正作用是**传播包含路径**。写成 `PRIVATE`，`apps` 就找不到 `<Eigen/Core>`
- 第三方库的头文件目录要标 `SYSTEM`，否则 `/W4` 下会被它的警告淹没
- 编译选项用 **INTERFACE 目标**而不是全局 `add_compile_options()`，
  否则会污染第三方库的警告输出
- 源文件列表**显式列出，不要用 `file(GLOB)`**（新增文件不会自动触发重新配置）

### Windows 编码

- **中文不能用作测试名 / 标签**。Windows 的 `main(argc, argv)` 收到的是
  ANSI 代码页（GBK）编码，而 `/utf-8` 编译的程序内字符串是 UTF-8 字节
  → 字节比较失败 → `No test cases matched`
- **纪律：代码标识符用英文，注释和文档用中文**
  （测试名、标签、函数名、变量名 → 英文；注释、文档、提交信息正文 → 中文）

### Visual Studio 工具问题（均非项目问题）

- **"IntelliSense 信息可能过时"** 是正常提示，重新生成即可。
  **红色波浪线 ≠ 编译错误**，最终裁判永远是编译器和链接器
- **测试资源管理器为空**：VS 的 `ExternalBuildProjectDiscoverer` 组件加载失败
  （`Microsoft.VisualStudio.Workspace.ExternalBuildFramework.Impl` 或其依赖找不到），
  属于 VS 安装缺陷。但 `out/build/x64-Debug/FEML.json` 里有**完全正确的测试描述**，
  证明 CMake 侧一切正常。
  **替代方案**：启动项设为 `feml_tests.exe` 按 F5，断点、单步、看变量全部可用

---

## 六、下一步：第 2 步 · 一维高斯积分

### 为什么先做这个

单元刚度矩阵的核心计算是积分：

$$K_e = \int_{-1}^{1} B^T D B \cdot |J| \, d\xi$$

对多项式单元，$B$、$D$ 都是多项式，**积分精度直接决定刚度阵是否正确**。
而"完全积分 vs 减缩积分"的差别，本质上就是积分点数的差别。

### 核心理论

**n 点高斯-勒让德积分对 2n−1 次多项式精确。**

| n（点数） | 精确到 |
|---|---|
| 1 | 1 次 |
| 2 | 3 次 |
| 3 | 5 次 |
| 4 | 7 次 |

**这直接决定梁单元各项该取几个点**：

- 弯曲项 κ = dθ/dx：2 节点单元 θ 线性 → κ 是**常数**（0 次）→ **1 点即精确**
- 剪切项 γ = dw/dx − θ：w、θ 均线性 → γ 是**线性**（1 次）→ **必须 2 点**

**这个次数差异就是剪切锁死的根源。**

### 待写文件

| # | 文件 | 状态 |
|---|---|---|
| 1 | `include/feml/element/integration_rule.hpp` | 内容已确定，**尚未创建** |
| 2 | `src/element/integration_rule.cpp` | 待写（高斯点与权重数值） |
| 3 | `tests/test_integration_rule.cpp` | 待写（用数学性质验证） |
| 4 | `src/CMakeLists.txt` | 待改（加新源文件） |
| 5 | `tests/CMakeLists.txt` | 待改（加新测试文件） |

### 高斯点与权重（实现时用）

| n | ξ | 权重 w |
|---|---|---|
| 1 | 0 | 2 |
| 2 | ±1/√3 ≈ ±0.5773502691896257 | 1 |
| 3 | 0 | 8/9 |
| 3 | ±√(3/5) ≈ ±0.7745966692414834 | 5/9 |
| 4 | ±0.3399810435848563 | 0.6521451548625461 |
| 4 | ±0.8611363115940526 | 0.3478548451374538 |

### 验证思路

用"n 点精确到 2n−1 次"这条性质自检：

- n=1 应精确积分 $\int_{-1}^{1} 1\,d\xi = 2$ 和 $\int_{-1}^{1}\xi\,d\xi = 0$，
  但**不**精确积分 $\int_{-1}^{1}\xi^2 d\xi = 2/3$
- n=2 应精确积分到 $\xi^3$
- n=3 应精确积分到 $\xi^5$（含 $\int_{-1}^{1}\xi^4 d\xi = 2/5$）
- n=4 应精确积分到 $\xi^7$

这是纯数学验证，不涉及任何有限元知识，非常适合作为第一个练手对象。

---

## 七、后续路线图

```
3. 单元抽象 + 注册表（Element 基类、ElementContext、ElementRegistry）
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
