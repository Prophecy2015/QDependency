cmake_minimum_required(VERSION 3.21)
project(QDepends VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_PREFIX_PATH)
    set(CMAKE_PREFIX_PATH "D:/DevelopTools/Qt/Qt6.11")
endif()

find_package(Qt6 REQUIRED COMPONENTS Core Widgets LinguistTools)
qt_standard_project_setup()

# ---- Capstone (x86/x64 disassembler) via FetchContent, static, x86 only ----
include(FetchContent)
set(CAPSTONE_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(CAPSTONE_BUILD_STATIC ON CACHE BOOL "" FORCE)
set(CAPSTONE_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(CAPSTONE_BUILD_CSTOOL OFF CACHE BOOL "" FORCE)
set(CAPSTONE_INSTALL OFF CACHE BOOL "" FORCE)
set(CAPSTONE_ARCHITECTURE_DEFAULT OFF CACHE BOOL "" FORCE)
set(CAPSTONE_X86_SUPPORT ON CACHE BOOL "" FORCE)
FetchContent_Declare(capstone
    GIT_REPOSITORY https://github.com/capstone-engine/capstone.git
    GIT_TAG 5.0.1
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(capstone)

# ---- core: PE parsing / loader resolution, QtCore only ----
set(CORE_SOURCES
    src/core/peinfo.h
    src/core/peparser.h
    src/core/peparser.cpp
    src/core/checksum.h
    src/core/checksum.cpp
    src/core/demangle.h
    src/core/demangle.cpp
    src/core/apiset.h
    src/core/apiset.cpp
    src/core/knowndlls.h
    src/core/knowndlls.cpp
    src/core/manifest.h
    src/core/manifest.cpp
    src/core/resolver.h
    src/core/resolver.cpp
)

set(SESSION_SOURCES
    src/session/modulenode.h
    src/session/analysissession.h
    src/session/analysissession.cpp
    src/session/importmodel.h
    src/session/importmodel.cpp
    src/session/exportmodel.h
    src/session/exportmodel.cpp
    src/session/modulelistmodel.h
    src/session/modulelistmodel.cpp
    src/session/sessionserializer.h
    src/session/sessionserializer.cpp
)

set(UI_SOURCES
    src/ui/mainwindow.h
    src/ui/mainwindow.cpp
    src/ui/iconfactory.h
    src/ui/iconfactory.cpp
    src/ui/panetitlebar.h
    src/ui/panetitlebar.cpp
    src/ui/findbar.h
    src/ui/findbar.cpp
    src/ui/sysinfodialog.cpp
    src/ui/sysinfodialog.h
    src/ui/logwidget.h
    src/ui/logwidget.cpp
)

# GUI target is skipped until its sources exist (lets the core layer build first)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/mainwindow.cpp)
    qt_add_executable(qdepends WIN32
        src/main.cpp
        ${CORE_SOURCES}
        ${SESSION_SOURCES}
        ${UI_SOURCES}
        # disassembly (links Capstone) — GUI target only, kept out of CORE_SOURCES
        src/core/disasm.h
        src/core/disasm.cpp
        src/ui/disasmdialog.h
        src/ui/disasmdialog.cpp
    )

    qt_add_resources(qdepends "theme"
        PREFIX "/"
        FILES resources/theme/dark.qss
    )

    target_include_directories(qdepends PRIVATE src)
    target_link_libraries(qdepends PRIVATE Qt6::Widgets dbghelp version capstone)
    target_include_directories(qdepends PRIVATE ${capstone_SOURCE_DIR}/include)

    qt_add_translations(qdepends
        TS_FILES resources/i18n/qdepends_zh_CN.ts
        RESOURCE_PREFIX "/i18n"
    )
endif()

# ---- console verification tool for the core layer ----
qt_add_executable(pedump
    tests/pedump.cpp
    ${CORE_SOURCES}
    src/session/modulenode.h
    src/session/analysissession.h
    src/session/analysissession.cpp
    src/session/sessionserializer.h
    src/session/sessionserializer.cpp
)
target_include_directories(pedump PRIVATE src)
target_link_libraries(pedump PRIVATE Qt6::Core dbghelp version)
