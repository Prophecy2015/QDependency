# QDepends — Dependency Walker 克隆 设计文档

> 版本: v1.0（已确认）
> 日期: 2026-06-11
> 已确认决策：**仅静态分析**（裁剪 Profiling，§2.6/M5 作废）；**界面英文为基础 + Qt Linguist 中文翻译包**（跟随系统语言）；**构建系统 CMake**。
> 目标: 使用 C++ / Qt 6.11 (MinGW64) 实现功能对齐官方 Dependency Walker (depends.exe 2.2) 的 PE 依赖分析工具，界面采用 VS Code 深色风格。

---

## 1. 项目概述

QDepends 是一个 Windows 平台的 PE 文件依赖分析工具，用于：

- 打开 32 位 (PE32) 和 64 位 (PE32+) 的 PE 文件（exe / dll / sys / ocx / cpl / drv / scr 等）；
- 递归解析其静态依赖（导入表、延迟加载表），构建完整的模块依赖树；
- 展示每个模块的导入 / 导出函数、文件属性、错误与警告；
- 模拟 Windows 加载器的 DLL 搜索顺序，准确定位每个依赖模块的实际路径；
- （第二阶段）运行时 Profiling：以调试器方式启动目标程序，跟踪动态 LoadLibrary / GetProcAddress 行为。

## 2. 功能需求（对齐 depends.exe）

### 2.1 文件打开

| 功能 | 说明 |
|---|---|
| 打开文件 | 菜单 / 工具栏 / 拖放 / 命令行参数打开 PE 文件 |
| 32/64 位支持 | 同时支持 PE32 与 PE32+，自动识别 Machine 类型 (x86 / x64 / ARM64 显示，x86/x64 完整解析) |
| 最近文件 | 最近打开列表（File 菜单） |
| 会话保存 | 将分析结果保存 / 重新加载（自定义 JSON 格式，替代 .dwi） |

### 2.2 五窗格主界面（与 depends.exe 布局一致）

```
┌──────────────┬─────────────────────────────────────┐
│              │  Parent Imports（父模块导入函数表）   │
│  Module      ├─────────────────────────────────────┤
│  Dependency  │  Exports（选中模块导出函数表）        │
│  Tree        │                                     │
│  (树视图)     │                                     │
├──────────────┴─────────────────────────────────────┤
│  Module List（全部模块平铺列表，含详细属性列）        │
├────────────────────────────────────────────────────┤
│  Log（错误 / 警告 / 信息日志）                       │
└────────────────────────────────────────────────────┘
```

**(1) 模块依赖树**
- 层级展示依赖关系；同一模块重复出现时标记为"重复"（灰色图标），不再向下展开；
- 模块状态图标对齐 depends.exe 语义：
  - 正常模块 / 缺失模块（红色）/ 无效 PE（红色感叹）/ 延迟加载（沙漏角标）/ 转发模块（F 角标）/ 重复模块（灰）/ 缺失导出符号的模块（红色警告）/ 64↔32 位混用（黄色警告）；
- 右键菜单：展开/折叠、定位到模块列表、在资源管理器中打开、复制完整路径、属性。

**(2) Parent Imports 窗格**：选中树节点后，显示其父模块从它导入的函数 — 列：PI 图标（按序号/名称/未找到）、Ordinal、Hint、Function、Entry Point。导入未在子模块导出表中找到时红色高亮并记录错误（对齐 depends 的 "unresolved import" 检测）。

**(3) Exports 窗格**：选中模块的全部导出 — 列：E 图标、Ordinal、Hint、Function、Entry Point（RVA 或转发字符串 `NTDLL.RtlAllocateHeap` 形式）。被父模块实际使用的导出加高亮标记。

**(4) Module List 窗格**：所有已解析模块的平铺去重列表，列对齐 depends.exe：
`图标 | Module | File Time Stamp | Link Time Stamp | File Size | Attr. | Link Checksum | Real Checksum | CPU | Subsystem | Symbols | Preferred Base | Actual Base | Virtual Size | Load Order | File Ver | Product Ver | Image Ver | Linker Ver | OS Ver | Subsystem Ver`
（静态分析下 Actual Base / Load Order 显示 N/A，Profiling 阶段填充。）

**(5) Log 窗格**：错误（缺失模块、未解析导入、CPU 类型不匹配、校验和不符等）、警告、Profiling 输出。

### 2.3 函数名处理

- C++ 修饰名反修饰开关（View → Undecorate）：
  - MSVC 修饰名（`?Foo@@YAXXZ`）→ 调用 `dbghelp.dll!UnDecorateSymbolName`；
  - GCC/MinGW 修饰名（`_ZN3FooC1Ev`）→ `abi::__cxa_demangle`（depends.exe 没有，属增强）；
- 完整路径 / 仅文件名切换（View → Full Paths）。

### 2.4 依赖解析（模拟 Windows Loader 搜索顺序）

按以下顺序定位依赖（对齐现代 Windows 加载器，优于 depends.exe 的旧逻辑）：

1. **API Set**（`api-ms-win-*` / `ext-ms-*`）→ 解析 `apisetschema.dll` 的 ApiSetMap 重定向到宿主 DLL；
2. **SxS / 应用清单**（嵌入 manifest 资源解析，WinSxS 程序集定位）；
3. **KnownDLLs**（注册表 `HKLM\SYSTEM\CurrentControlSet\Control\Session Manager\KnownDLLs`）；
4. 被分析文件所在目录；
5. 系统目录：被分析文件为 64 位 → `System32`；32 位 → `SysWOW64`（注意：本程序自身为 64 位，访问 SysWOW64 不受文件系统重定向影响，需用真实路径）；
6. Windows 目录；
7. 当前目录；
8. `PATH` 环境变量各目录。

其他要求：
- 循环依赖检测与终止；模块按"小写文件名+路径"全局缓存，去重；
- 转发导出（forwarded export）追加目标模块进树（depends.exe 行为）；
- 延迟加载导入表（Delay Import Descriptor）单独解析并以沙漏图标标注；
- Bound Import 信息读取展示；
- .NET 程序（CLR 头存在）识别并提示（对齐 depends 对托管程序的处理：仅分析原生导入并警告）。

### 2.5 其他对齐功能

- 全文搜索（Edit → Find，在函数列表中查找）；
- 列排序（所有表格点击表头排序，对齐 depends 的排序菜单）；
- 复制（单元格 / 整行 / 树路径）；
- 导出报告：纯文本（对齐 depends 的 Save As Text）与 CSV；
- View → System Information 对话框（OS 版本、处理器、目录、环境变量）；
- View → Refresh (F5) 重新分析；
- External Viewer 配置（用外部工具打开模块）；
- 状态栏显示当前分析状态 / 选中模块路径。

### 2.6 Profiling

**已确认裁剪**：本项目仅做静态分析，不实现运行时动态跟踪，菜单中不出现 Profile 相关功能。Module List 中的 Actual Base / Load Order 列恒显示 N/A（保留列以对齐官方界面）。

## 3. 架构设计

### 3.1 分层

```
┌────────────────────────────────────────────┐
│ UI 层 (QtWidgets)                           │
│  MainWindow / TreePane / ImportPane /       │
│  ExportPane / ModuleListPane / LogPane /    │
│  ThemeManager(QSS) / SysInfoDialog          │
├────────────────────────────────────────────┤
│ 会话层 (QtCore)                              │
│  AnalysisSession: 驱动递归解析(后台线程)、    │
│  模块缓存、树/列表数据模型(QAbstractItemModel)│
├────────────────────────────────────────────┤
│ 核心层 (纯 C++ + 少量 Win32, 不依赖 QtGui)    │
│  peparser: PE32/PE32+ 解析(内存映射只读)      │
│  resolver: Loader 搜索顺序 / ApiSet /        │
│            KnownDLLs / Manifest             │
│  demangle: UnDecorateSymbolName +           │
│            __cxa_demangle 封装               │
└────────────────────────────────────────────┘
```

核心层不含 UI 依赖，便于单元测试（计划用 Qt Test 对 peparser/resolver 做用例覆盖，测试样本用系统自带 DLL）。

### 3.2 核心数据结构（示意）

```cpp
enum class CpuType { X86, X64, Arm64, Other };
enum class ModuleStatus { Ok, MissingFile, InvalidPE, WrongCpu, ExportMissing, ... };

struct ExportEntry  { uint32_t ordinal, hint; std::string name; uint32_t rva;
                      std::string forwardTarget; bool isForwarded; };
struct ImportEntry  { bool byOrdinal; uint32_t ordinal, hint; std::string name;
                      bool resolved;          // 是否在子模块导出表中找到
                      bool isDelayLoad; };
struct PeInfo       { CpuType cpu; bool isPe64, isDotNet;
                      uint64_t preferredBase; uint32_t virtualSize, linkChecksum, realChecksum;
                      时间戳/子系统/版本信息等; std::vector<ExportEntry> exports;
                      std::vector<ImportDescriptor> imports, delayImports; };
struct ModuleNode   { QString resolvedPath; ModuleStatus status;
                      std::shared_ptr<PeInfo> pe;   // 同一文件全局共享
                      std::vector<ImportEntry> parentImports;  // 相对父节点
                      bool isDuplicate, isDelayLoad, isForwarder;
                      std::vector<std::unique_ptr<ModuleNode>> children; };
```

### 3.3 解析流程

1. UI 线程接收文件 → 投递到后台线程（`QThread` + worker，避免界面卡顿）；
2. 解析根文件 PE → 收集 imports/delayImports → 逐个经 resolver 定位路径；
3. 已在全局缓存（按规范化路径）→ 标记 duplicate，不展开；否则递归（深度优先，循环检测）；
4. 对每条导入在子模块导出表中匹配（名称 / 序号），未命中→ 错误；
5. 转发导出解析出目标模块名并补充为子节点；
6. 进度与日志经信号增量回传 UI；完成后一次性构建模型（大树性能考虑）。

### 3.4 PE 解析要点

- `QFile::map` 内存映射只读解析，**绝不 LoadLibrary 目标文件**（安全 + 可解析异架构文件）；
- 统一用 `IMAGE_NT_HEADERS32/64` 双分支（`Magic` 判定 0x10B / 0x20B），数据目录读取封装 `rva→fileOffset`（段表遍历）；
- 解析目录：Import、Delay Import、Export、Bound Import、Resource（仅 VS_VERSION_INFO 与 RT_MANIFEST）、CLR Descriptor、TLS（存在性展示）；
- Real Checksum 自行按 PE 校验和算法计算（不依赖 imagehlp，避免位数限制）；
- 容错：截断文件、畸形 RVA、越界指针全部边界检查，解析失败标记 InvalidPE 并继续整体流程。

## 4. UI / 视觉设计（VS Code Dark+ 风格）

### 4.1 配色（QSS 全局样式表）

| 元素 | 颜色 |
|---|---|
| 主背景 | `#1e1e1e` |
| 侧栏/窗格标题/表头 | `#252526` |
| 边框/分割线 | `#3c3c3c` |
| 正文文字 | `#d4d4d4` |
| 次要文字 | `#858585` |
| 强调/选中 | `#094771`（选中底）+ `#007acc`（焦点） |
| 错误 | `#f48771`  警告 | `#cca700`  成功 | `#89d185` |
| 状态栏 | `#007acc` 蓝底白字（VS Code 标志性） |

### 4.2 风格细节

- 窗格采用 `QSplitter`（细分割条，hover 高亮 `#007acc`），各窗格带 VS Code 风格小标题栏（大写灰字，如 `DEPENDENCY TREE`）；
- 表格/树：无网格线、行 hover `#2a2d2e`、扁平表头、细滚动条（hover 加宽）；
- 字体：界面 `Segoe UI` 9pt，函数/路径等数据列 `Consolas` / `Cascadia Mono`；
- 图标：SVG 绘制单色线性图标（VS Code Codicons 风格），模块状态用颜色+角标区分；
- 菜单栏深色、工具栏扁平图标按钮；支持 F11 全屏、各窗格可隐藏（View 菜单）。

## 5. 技术选型与构建

| 项 | 选择 |
|---|---|
| 语言标准 | C++20 |
| GUI | Qt 6.11 Widgets（路径 `D:\DevelopTools\Qt\Qt6.11`） |
| 编译器 | MinGW64 (`D:\DevelopTools\mingw64`)，64 位单一目标 |
| 构建 | CMake + mingw32-make（`CMAKE_PREFIX_PATH=D:/DevelopTools/Qt/Qt6.11`） |
| 系统库 | `dbghelp`（UnDecorateSymbolName）、`version`、`advapi32`（注册表） |
| 测试 | Qt Test（核心层单测） |
| 部署 | `windeployqt` 生成绿色目录 |

> 说明：程序本体编译为 64 位即可——PE 解析是纯文件级操作，与目标文件位数无关，因此单一 64 位程序即可"打开 32 位和 64 位 PE 文件"。仅 Profiling 依赖调试 API，64 位进程亦可调试 WOW64 进程。

## 6. 工程结构

```
Dependency/
├─ CMakeLists.txt
├─ src/
│  ├─ core/        # peparser.{h,cpp} peinfo.h resolver.{h,cpp} apiset.{h,cpp}
│  │               # knowndlls.{h,cpp} checksum.cpp demangle.{h,cpp}
│  ├─ session/     # analysissession.{h,cpp} modulenode.h treemodel.cpp
│  │               # importmodel.cpp exportmodel.cpp modulelistmodel.cpp logmodel.cpp
│  ├─ ui/          # mainwindow.{h,cpp} panes/*.cpp sysinfodialog.cpp
│  │               # thememanager.cpp searchbar.cpp
│  └─ main.cpp
├─ resources/      # theme/dark.qss  翻译 zh_CN.ts  app.rc(图标/版本)
├─ tests/          # tst_peparser.cpp tst_resolver.cpp
└─ DESIGN.md
```

## 7. 里程碑

| 阶段 | 内容 | 交付 |
|---|---|---|
| M1 | 核心层：PE32/PE32+ 解析、导入导出、校验和、版本资源 + 单元测试 | 命令行可验证 |
| M2 | resolver（ApiSet/KnownDLLs/搜索顺序）+ 递归依赖树（后台线程） | 数据完整 |
| M3 | 五窗格 UI + 深色主题 + 排序/搜索/复制/导出/会话保存 | 可用版本 |
| M4 | 细节对齐：转发链、清单/SxS、System Info、External Viewer、中文翻译包 | 功能对齐版（最终交付） |

## 8. 风险与裁剪说明

1. **Profiling / GetProcAddress hook**：已确认整体裁剪，不实现；
2. ARM64 PE 可识别并展示，但依赖搜索路径按 x64 规则处理（depends.exe 本身不支持 ARM64，属超出对齐范围的展示性支持）；
3. depends.exe 的 `.dwi` 二进制会话格式不公开，采用自有 JSON 格式替代（不做 .dwi 互导）；
4. WinSxS 清单解析仅覆盖常见 `dependentAssembly` 场景，复杂策略重定向（publisher policy）按尽力而为处理。
