# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

QDepends — a static PE dependency analyzer (Dependency Walker / depends.exe clone) in C++20 / Qt 6 Widgets with a VS Code Dark+ themed UI. Windows-only. Confirmed scope decisions: **static analysis only** (no runtime profiling — do not add it), English UI strings with a zh_CN translation pack, CMake build. See DESIGN.md for the agreed feature set.

## Build & Run

Qt is at `D:\DevelopTools\Qt\Qt6.11` (kit dir directly — bin/include/lib at the root, not the installer layout). MinGW64 is at `D:\DevelopTools\mingw64`. CMakeLists falls back to that Qt path automatically if `CMAKE_PREFIX_PATH` is unset.

```powershell
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build -j 8                          # both targets
cmake --build build --target qdepends -j 8        # GUI only
cmake --build build --target pedump -j 8          # CLI tool only
```

Running exes from `build/` requires Qt + MinGW on PATH:

```powershell
$env:PATH = "D:\DevelopTools\Qt\Qt6.11\bin;D:\DevelopTools\mingw64\bin;" + $env:PATH
```

A build failure with "Error 1" on `qdepends.exe` and no compile error usually means the app is still running (file locked): `Get-Process qdepends | Stop-Process -Force`.

Deployment (standalone dir in `dist/`): `windeployqt.exe --release --compiler-runtime dist\qdepends.exe`.

## Testing

There is no unit-test framework; `pedump.exe` is the regression harness for everything below the UI:

```powershell
.\build\pedump.exe C:\Windows\System32\notepad.exe --resolve    # per-import resolution + search-order notes
.\build\pedump.exe C:\Windows\SysWOW64\notepad.exe --tree       # full recursive analysis (32-bit path)
.\build\pedump.exe <file> --roundtrip                            # session save/load consistency
.\build\pedump.exe <file> --imports --exports                    # raw table dumps + demangling
```

Expected baseline on a healthy system: `--tree` on System32/SysWOW64 notepad.exe ends with `errors=0` and `unresolved-import-refs=0` (warnings are normal — optional `ext-ms-*` contracts). Any regression in the parser/resolver shows up there first. Test both bitnesses: 32-bit exercises WOW64 redirection, 64-bit does not.

## Architecture

Three layers, dependency direction strictly downward:

- **`src/core/`** — PE parsing and loader simulation. QtCore-only (no QtGui/Widgets). `peparser.cpp` parses PE32/PE32+ via read-only memory mapping and **never LoadLibrary's the target** (that's why one 64-bit binary handles both bitnesses). `resolver.cpp` simulates the Windows loader search order: SxS manifest dirs → KnownDLLs → app dir → System32/SysWOW64 (by *importer* bitness, with WOW64 file-system redirection for 32-bit importers — a critical correctness rule) → Windows dir → PATH. `apiset.cpp` reads the ApiSet schema **from this process's own PEB** (offset 0x68), so `api-ms-*`/`ext-ms-*` contracts resolve like the live OS does.
- **`src/session/`** — `analysissession.cpp` runs the recursive tree build on a `QThread` (results delivered via queued signal carrying `AnalysisResultPtr`). Modules are globally deduped by lowercase resolved path: first occurrence expands, later ones are flagged `duplicate` with no children. Forwarded exports actually used by a parent become extra child nodes. The three `QAbstractTableModel`s expose a `SortRole` (UserRole+1) with numeric sort keys; views sort through `QSortFilterProxyModel` with that role. `sessionserializer.cpp` is the `.qds` JSON format (PeInfo stored once per unique module, tree references by key).
- **`src/ui/`** — five-pane `MainWindow` (tree / parent imports / exports / module list / log) in nested `QSplitter`s. Theme = Fusion style + dark palette (`main.cpp`) + `resources/theme/dark.qss`. All icons are drawn at runtime with QPainter (`iconfactory.cpp`); toolbar glyphs use the Segoe MDL2 Assets font. Tree items carry a raw `ModuleNode*` in `Qt::UserRole` — nodes are owned by the `AnalysisResult` kept alive in `m_result`.

Layering exception: session models include `ui/iconfactory.h` for DecorationRole icons. `pedump` compiles core + `analysissession`/`sessionserializer` only (no models — they'd pull in QtGui).

## Gotchas

- Qt's `slots`/`signals`/`emit` macros: never use them as identifiers (a `std::vector slots(...)` variable once broke the build with baffling parse errors).
- Error-vs-warning semantics mirror depends.exe: missing *implicit* dependency = Error (red), missing *delay-load* dependency = Warning (yellow). Keep new diagnostics consistent with this.
- depends.exe `.dwi` session format is intentionally not supported; `.qds` JSON replaces it.

## Translations

UI strings are English in `tr()`; Chinese lives in `resources/i18n/qdepends_zh_CN.ts` (compiled to .qm automatically by the build, embedded under `:/i18n`). After changing any user-visible string:

```powershell
cmake --build build --target update_translations          # lupdate
powershell -File tools\fill_translations.ps1               # fills from the dictionary in that script — add new entries there
cmake --build build
```

The fill script must stay UTF-8 **with BOM** or PowerShell 5.1 mis-parses the Chinese literals.
