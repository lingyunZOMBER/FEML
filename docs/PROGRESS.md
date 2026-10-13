# FEML 开发进度

> 本文档记录**当前状态、技术决策、路线图和下一步计划**。
> 踩坑经验单独放在 [LESSONS.md](LESSONS.md)。
> 每次开发告一段落时更新。

**最后更新**：2026-10-13

---

## 一、当前状态

| 项目 | 状态 |
|---|---|
| 仓库 | https://github.com/lingyunZOMBER/FEML （`main` 分支） |
| 工程 | 可配置 ✅ 可编译 ✅ 可运行 ✅ 可测试 ✅ |
| 测试 | **16 个用例、73 个断言全部通过** |
| 进度 | 第 1~3 步完成 → **第 4 步待开始** |

**当前能跑的东西**

```
bin/feml.exe         → 打印 "FEML 0.1.0" / "Eigen 3.4.1"
bin/feml_tests.exe   → 16 个用例通过
```

---

## 二、快速命令参考

### 在 Visual Studio 里（推荐路径）

| 操作 | 方式 |
|---|---|
| 配置 | **项目 → 配置缓存**（保存 `CMakeLists.txt` 后通常自动触发） |
| 彻底重来 | **项目 → 删除缓存并重新配置** |
| 编译 | **生成 → 生成全部**（`Ctrl+Shift+B`） |
| 运行主程序 | 启动项选 `feml_main.exe` → **Ctrl+F5** |
| 跑全部测试 | 启动项选 **`feml_tests.exe`** → **Ctrl+F5**（不带调试器）或 **F5**（带调试器，可打断点） |
| 只跑某类测试 | **项目 → 调试和启动设置**，给 `feml_tests.exe` 加命令行参数如 `[element]` |
| 看 CMake 日志 | 视图 → 输出，来源选 **CMake** |
| 看编译日志 | 视图 → 输出，来源选 **生成** |

> 构建目录是 `out/build/x64-Debug`（VS 的默认配置）。
> 产物在 `out/build/x64-Debug/bin/` 和 `lib/`。

### 命令行（备选）

编译（普通终端里需要先注入 MSVC 环境变量，项目里备了脚本）：

```
cmd /c "F:\vs project\FEML\out\build\build_feml.bat"
```

跑测试（不需要 MSVC 环境）：

```
cd /d "F:\vs project\FEML"
"F:\visual studio\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe" ^
    --test-dir out\build\x64-Debug --output-on-failure
```

---

## 三、已完成的工作

### 提交历史

```
fef5138  feat: 添加 core 基础头文件，实现一维高斯积分规则与积分方案解析
b758c94  docs: 添加开发进度文档，记录已完成工作、决策与踩坑经验
62acc2c  test: 接入 Catch2 与 CTest，添加版本号冒烟测试
9eaeba6  feat: 搭建 CMake 工程骨架，接通 Eigen 与 Catch2
9ae335c  Update .gitignore
3e06923  Initial commit
```

### 文件清单

```
CMakeLists.txt                             工程入口
cmake/Dependencies.cmake                   依赖管理（Eigen + Catch2，FetchContent）

include/feml/core/types.hpp                地基：Real / Index / 常量
include/feml/core/exceptions.hpp           异常层次
include/feml/core/version.hpp              版本号
include/feml/model/material.hpp            材料（E、ν、剪切模量）
include/feml/model/section.hpp             截面（A、I、剪切修正因子、厚度）
include/feml/element/integration_rule.hpp  高斯积分点、积分规则、积分方案
include/feml/element/element.hpp           ★ Element 抽象基类、ElementContext
include/feml/element/registry.hpp          单元注册表

src/core/version.cpp
src/element/integration_rule.cpp           高斯点与权重、方案关键字解析
src/element/registry.cpp                   注册表实现
src/CMakeLists.txt

apps/feml_main.cpp                         主程序
apps/CMakeLists.txt

tests/test_smoke.cpp                       1 个用例
tests/test_integration_rule.cpp            7 个用例
tests/test_element_interface.cpp           8 个用例
tests/CMakeLists.txt

docs/PROGRESS.md                           本文档
docs/LESSONS.md                            踩坑记录
.gitignore / .gitattributes / LICENSE / README.md
第2次作业.doc / 第3次作业.doc
```

### 目录结构约定

```
FEML/
├─ CMakeLists.txt          工程入口
├─ cmake/                  构建辅助脚本
├─ include/feml/           公开头文件（include 根 = 命名空间路径）
│  ├─ core/                基础类型、异常、版本
│  ├─ model/               问题数据（材料、截面）
│  └─ element/             单元基类、积分、注册表
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
| 语言标准 | **C++20** | `std::format`、`inline constexpr`、`enum class`、`std::numbers` |
| 构建系统 | **CMake ≥3.25 + Ninja**（VS 内置） | 跨平台、VS 原生支持、随时可生成 .sln |
| 线性代数 | **Eigen 3.4.1** | 保留 `LinearSolver` 抽象层，日后自己实现可对照 |
| 测试框架 | **Catch2 v3.9.1 + CTest** | 单头文件；`catch_discover_tests` 让每个用例独立可见 |
| 依赖获取 | **FetchContent + 锁定 URL/SHA256** | 版本写在代码里，任何人 clone 都能复现 |
| 架构分层 | apps → analysis → assembly → element → model → linalg → core | 依赖严格单向 |

**锁定的依赖版本**

```cmake
# Eigen 3.4.1
URL      https://gitlab.com/libeigen/eigen/-/archive/3.4.1/eigen-3.4.1.tar.gz
URL_HASH SHA256=b93c667d1b69265cdb4d9f30ec21f8facbbe8b307cf34c0b9942834c6d4fdbe2

# Catch2 v3.9.1
URL      https://github.com/catchorg/Catch2/archive/refs/tags/v3.9.1.tar.gz
URL_HASH SHA256=a215c2a723bd7483efd236dc86066842a389cb4e344c61119c978acdf24d39be
```

**贯穿全局的设计原则**

1. `Element` 多态基类 + 注册表 —— 新增单元只写一个文件，不改已有代码
2. 积分方案是**运行时参数**，不是编译期分支（两次作业都要对比积分方案）
3. `DofManager` 管"节点分量 → 方程号" —— **必须从一开始就做成单元无关的**
   （梁 2 自由度/节点，板 3 自由度/节点）
4. `LinearSolver` 抽象接口 —— 求解器可替换

**编码纪律**

- **代码标识符用英文，注释和文档用中文**
- **头文件里禁止 `using namespace std;`**，一律写 `std::` 前缀
- **包含你所使用的东西**（用了 `std::find` 就包含 `<algorithm>`，不靠间接包含）

---

## 五、路线图

三次作业的主线是同一条：**弯曲与剪切刚度的量阶失配导致的剪切锁死，以及积分方案如何解决它**。

| 作业 | 现象 | 机制 |
|---|---|---|
| 2（梁） | 剪切锁死 | Φ = 12EI/(kGAL²) ∝ (h/L)² → 0 时锁死 |
| 3（板） | 剪切锁死 | 弯曲刚度 ∝ t³，剪切刚度 ∝ t → 比值 ∝ 1/t²，t→0 时锁死 |

```
✅ 1. 工程骨架（CMake + Eigen + Catch2 + 测试框架）
✅ 2. 一维高斯积分规则
✅ 3. 单元抽象 + 注册表（Element 基类、ElementContext、ElementRegistry）
   4. 2 节点 Timoshenko 梁单元          ← 作业 2 核心
   5. 装配 + 约束施加 + 线性求解
        ⚠ DofManager 必须通用化（为作业 3 的板单元预留 3 自由度/节点）
   6. 3 节点 Timoshenko 梁单元          ← 作业 2 要求 2
   7. 卡片式输入 + 结果输出
        ⚠ 节点坐标要支持 2 维（为作业 3 预留）
   8. 细长比扫描 + 作业 2 报告
   ────────────── 作业 2 完成 ──────────────
   9. 2D 高斯积分 + 等参变换 / Jacobian  ← 作业 3 地基（可提前做）
  10. Mindlin 板单元（Lagrange 9 / Serendipity 8）
  11. 三种积分方案对比 + 作业 3 报告
```

### 作业 3 的额外需求

**两个题目任选一题**：

1. 编写**二次 Lagrange 板单元**和 **Serendipity 板单元**，参照《有限单元法》图 10.12
   的算例比较**精确积分 / 减缩积分 / 选择积分**三种方案 ← **推荐**
2. 编写 **8 节点假设剪切应变 Mindlin 单元**，用图 10.15 校核

**推荐题目 1** 的理由：直接复用 `IntegrationScheme`（第 2 步就建好了）；
两种单元共享约 80% 代码（都是 8/9 节点 Mindlin 板，只差形函数和节点数），
工作量约为题目 2 的 1.5 倍而非 2 倍；"Lagrange vs Serendipity" 本身值得讨论。

**相对第 2 次作业需要新增的模块**：

| # | 新增内容 | 说明 |
|---|---|---|
| 1 | **2D 高斯积分** | `GaussRule1D` 只是 1D，2D 规则 = 两个 1D 的张量积 |
| 2 | **2D 形函数** | Lagrange 9 节点、Serendipity 8 节点的 N(ξ,η) 及偏导 |
| 3 | **等参变换 / Jacobian** | 2D 下需要 2×2 的 J 矩阵及行列式、逆矩阵 ← 最大的新增概念 |
| 4 | **板的本构** | 弯曲刚度 $D_b \propto \frac{Et^3}{12(1-\nu^2)}$、剪切刚度 $D_s \propto kGt$ |
| 5 | **2D 输入格式** | 节点坐标是二维的 |
| 6 | 广义应变输出 | 板的"应变"是曲率 κ 和剪切应变 γ，"应力"是弯矩和剪力 |

**现有架构已经支持的部分**（不用改）：`spatialDim()=2`、`dofPerNode()=3`、
`node_coords` 是 n×2 矩阵、三种积分方案齐全、`Section::thickness` 已有。

---

## 六、下一步：第 4 步 · 2 节点 Timoshenko 梁单元

**这是全程最难的一步**——不是代码多，而是理论和实现直接耦合。

### 待写文件

| # | 文件 | 内容 |
|---|---|---|
| 1 | `include/feml/element/beam2_timoshenko.hpp` | 单元类声明 |
| 2 | `src/element/beam2_timoshenko.cpp` | B 矩阵、刚度阵积分 |
| 3 | `include/feml/element/register_builtin.hpp` + `src/element/register_builtin.cpp` | 显式注册点 |
| 4 | `tests/test_beam2.cpp` | 刚度阵性质测试 |
| 5 | 改 `apps/feml_main.cpp` | 调用 `registerBuiltinElements()` |
| 6 | 改两个 CMakeLists | 加源文件 |

### 核心理论

自由度 (w₁, θ₁, w₂, θ₂)：

$$\Phi = \frac{12EI}{kGA L^2} = \frac{E\,h^2}{k\,G\,L^2} \quad (\text{矩形截面 } k = 5/6)$$

$$K = \frac{EI}{L^3(1+\Phi)}
\begin{bmatrix}
12 & 6L & -12 & 6L \\
6L & (4+\Phi)L^2 & -6L & (2-\Phi)L^2 \\
-12 & -6L & 12 & -6L \\
6L & (2-\Phi)L^2 & -6L & (4+\Phi)L^2
\end{bmatrix}$$

Φ → 0 时退化为 Euler-Bernoulli 梁。

**积分阶次的推理链**（每次加新单元都要走一遍）：

```
形函数次数 → 应变分量次数 → B 矩阵次数 → B^T D B 次数 → 所需积分点数
```

注意 **$B^T D B$ 里 $B$ 被平方，次数翻倍**——这是决定积分阶次的关键。

| 单元 | 项 | B 的次数 | $B^T D B$ 的次数 | 精确所需点数 |
|---|---|---|---|---|
| 2 节点 | 弯曲 κ = dθ/dx | 0（常数） | 0 | 1 |
| 2 节点 | 剪切 γ = dw/dx − θ | 1（线性） | 2 | 2 |
| 3 节点 | 弯曲 κ = dθ/dx | 1（线性） | 2 | 2 |
| 3 节点 | 剪切 γ = dw/dx − θ | 2（二次） | 4 | 3 |

### 为什么完全积分会锁死（报告的核心论点）

2 节点单元 4 个自由度减 2 个刚体模态，**K 的秩只需要 2**。

- **1 点积分**：弯曲贡献秩 1、剪切贡献秩 1，总秩 2 ✓ 恰好
- **2 点积分**：弯曲贡献秩 1、剪切贡献秩 2，总秩 3 ✗ **多出一个虚假约束**

那个虚假约束强行压制弯曲变形 → 梁越细越锁死。

### 必做的三个测试

| 测试 | 依据 |
|---|---|
| **K 对称** | 刚度阵源于能量泛函的二阶导数，必然对称 |
| **刚体模态零能** | 平移 + 转动不产生节点力（2 个模态） |
| **零特征值个数 == 2** | 能挡住绝大部分零能模态 bug |

这三个是**不依赖具体数值的物理性质检查**，第 3 步的杆单元测试里已经建立了这个模式。

---

## 七、挂起的事项

1. **「细长比」的定义需要跟老师确认**
   - 若指 L/h，趋近 0 是粗短梁（剪切变形主导，差异来自剪切项）
   - 若指 h/L，趋近 0 是细长梁（Φ→0，正是剪切锁死发作处）
   - **对策**：程序把 h/L 做成扫描参数、覆盖两个方向，报告里两个极限都讨论

2. **《有限单元法》书上算例的参数要抄过来**
   - 作业 2："1 个单元的结果应该和书上算例相同"
   - 作业 3：图 10.12（或 10.15）的算例

3. **交作业前考虑把依赖 vendor 进仓库**
   - 当前 FetchContent 首次配置需要联网，国内访问 GitHub 不稳定
   - 方案：`git clone --depth 1` 到 `third_party/`，去掉 `.git` 后纳入仓库

4. **VS 测试资源管理器不可用**（VS 安装缺陷，非项目问题）
   - 替代方案：启动项选 `feml_tests.exe` → F5/Ctrl+F5
   - 或 VS Installer → 修改 → 修复

5. **`CMakePresets.json` 尚未创建**
   - 现在 VS 用自己的默认配置（`out/build/x64-Debug`）
   - 等到需要跑细长比扫描（性能敏感，要 Release）时再补
