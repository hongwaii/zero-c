# zero-c

基于 CMake + LLVM-MinGW 的 Windows 平台 C 语言编译框架，工具链以 zip 形式内置，克隆即用，首次运行将自动解压，请耐心等待。

## 快速开始

```batch
git clone git@github.com:hongwaii/zero-c.git
cd zero-c
.\build.bat
```

无需安装任何工具——编译器（MinGW）和构建工具（CMake）以 zip 形式内置，首次运行自动解压。

## 编译命令

| 命令 | 说明 |
|------|------|
| `.\build.bat` | 编译全部 → `out/APP-1.1.0.YYMMDDHHMM_alpha.exe` |
| `.\build.bat test` | 编译测试 → `out/TEST.exe` |
| `.\build.bat clean` | 清理 `out/` |
| `.\build.bat help` | 显示帮助 |

## 项目结构

```
├── build.bat              编译入口
├── CMakeLists.txt          根 CMake
├── version.h.in            版本头文件模板 → include/version.h
├── core/                   主程序 (main.c)
├── include/                公共头文件
├── lib/                    项目库 (.a / .dll)
├── midware/                中间件模块 (http, ...)
├── test/                   测试代码 (build.bat test 时编译)
├── third_party/            第三方库
├── tools/                  自包含工具链
│   ├── llvm-mingw-20260519-ucrt-x86_64.zip  # 首次运行自动解压
│   ├── llvm-mingw-20260519-ucrt-x86_64/      # (git 忽略)
│   ├── cmake-4.3.3-windows-x86_64.zip        # 首次运行自动解压
│   └── cmake-4.3.3-windows-x86_64/           # (git 忽略)
└── out/                    编译输出
```

## 版本号

编辑 `build.bat` 顶部：

```batch
set PRODUCT_NAME=APP
set VERSION_MAJOR=1
set VERSION_MINOR=1
set VERSION_PATCH=0
set BUILD_TYPE=alpha          # alpha | beta | release
```

编译时自动生成 `include/version.h`，程序可通过 `#include "version.h"` 获取：

```c
#define APP_FULL_TAG     "APP-1.1.0.2605301230_alpha"
#define APP_GIT_HASH     "a1b2c3d4"
#define APP_VERSION_STRING "APP-1.1.0.2605301230_alpha"
```

## 模块说明

### core/ — 主程序

添加源文件和依赖库，编辑 [core/CMakeLists.txt](core/CMakeLists.txt)。

### lib/ — 项目库

[lib/CMakeLists.txt](lib/CMakeLists.txt) 提供四种模板：`STATIC`（.a）、`SHARED`（.dll）、`IMPORTED`（预编译库）、工具链系统库。按需取消注释。

### midware/ — 中间件

每个模块独立解耦，统一模板：源文件 + 公共头文件 + 私有头文件 + 依赖库。添加模块：

1. 创建 `midware/<name>/CMakeLists.txt`（参考 http/）
2. 在 [midware/CMakeLists.txt](midware/CMakeLists.txt) 添加 `add_subdirectory(<name>)`
3. 在 [core/CMakeLists.txt](core/CMakeLists.txt) 链接 `midware_<name>`

### test/ — 测试

`build.bat test` 编译，输出 `out/TEST.exe`。提供 `RUN_TEST()` 宏和断言辅助。

### third_party/ — 第三方库

两种集成方案（详见 [third_party/CMakeLists.txt](third_party/CMakeLists.txt)）：
- **方案 A**：`add_subdirectory()` 使用上游 CMakeLists
- **方案 B**：手动列源文件，`add_library()` 直接编译

## 常见问题

| 问题 | 解决 |
|------|------|
| `gcc.exe not found` | 确保 `tools/llvm-mingw-*/bin/gcc.exe` 存在 |
| `cmake.exe not found` | 确保 `tools/cmake-*/bin/cmake.exe` 存在 |
| CMake 配置失败 | 检查 CMakeLists.txt 中列出的源文件是否存在 |
| 链接错误 | 检查 `target_link_libraries()` 依赖是否完整 |
| 解压失败 | 确保 电脑为Windows 10/11以上版本|
