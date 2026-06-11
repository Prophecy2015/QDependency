# QDepends

功能对齐官方 Dependency Walker (depends.exe) 的静态 PE 依赖分析工具，C++20 / Qt 6 Widgets 实现，VS Code Dark+ 风格界面。

![功能] 支持 32 位 (PE32) 与 64 位 (PE32+) 模块 · API Set 重定向 · KnownDLLs · WOW64 文件系统重定向 · SxS 清单 · 延迟加载 · 转发导出 · C++ 名称反修饰 (MSVC + GCC) · 会话保存 (.qds) · 文本/CSV 报告导出 · 英文界面 + 中文语言包（跟随系统）

## 构建

依赖：Qt 6.11（`D:\DevelopTools\Qt\Qt6.11`）、MinGW64、CMake ≥ 3.21。

```powershell
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build -j 8
```

产物：

- `build/qdepends.exe` — 主程序（GUI）
- `build/pedump.exe` — 核心层命令行验证工具（`--imports --exports --resolve --tree --roundtrip`）

## 部署（绿色目录）

```powershell
mkdir dist; copy build\qdepends.exe dist\
D:\DevelopTools\Qt\Qt6.11\bin\windeployqt.exe --release --compiler-runtime dist\qdepends.exe
```

## 翻译更新

```powershell
cmake --build build --target update_translations   # lupdate 抽取字符串
powershell -File tools\fill_translations.ps1        # 按字典填充中文
cmake --build build                                  # lrelease 自动编译 .qm
```

## 工程结构

```
src/core/     PE 解析(内存映射只读) / Loader 搜索顺序模拟 / ApiSet / KnownDLLs / 反修饰
src/session/  后台递归分析 / 依赖树 / 三大表格模型 / 会话序列化(JSON)
src/ui/       五窗格主窗口 / VS Code 深色主题(QSS) / QPainter 绘制图标
tests/        pedump 命令行验证工具
```

详见 [DESIGN.md](DESIGN.md)。
