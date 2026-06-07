@echo off
setlocal

REM ============================================================
REM  Windows-C Build Script
REM    build.bat             Build all   -> APP-<ver>_<type>.exe
REM    build.bat all         Same as above
REM    build.bat test        Build test  -> TEST.exe
REM    build.bat shell       Build shell-only (AGENT_SHELL_ONLY=1, mock backend)
REM    build.bat clean       Clean out/
REM    build.bat help        Show this help
REM ============================================================

REM ---- Product metadata ---------------------------------------
set PRODUCT_NAME=APP
set VERSION_MAJOR=1
set VERSION_MINOR=1
set VERSION_PATCH=0
set BUILD_TYPE=alpha

REM ---- Paths --------------------------------------------------
set "ROOT_DIR=%~dp0"
set "ROOT_DIR=%ROOT_DIR:~0,-1%"
set "TOOLS_DIR=%ROOT_DIR%\tools"
set "MINGW_PATH=%TOOLS_DIR%\llvm-mingw-20260519-ucrt-x86_64\bin"
set "MINGW_ZIP=%TOOLS_DIR%\llvm-mingw-20260519-ucrt-x86_64.zip"
set "CMAKE_BIN=%TOOLS_DIR%\cmake-4.3.3-windows-x86_64\bin"
set "CMAKE_ZIP=%TOOLS_DIR%\cmake-4.3.3-windows-x86_64.zip"
set "OUT_DIR=%ROOT_DIR%\out"
set "BUILD_DIR=%OUT_DIR%\build"

REM ---- Build timestamp: YYMMDDHHMM ----------------------------
for /f %%I in ('powershell -Command "Get-Date -Format 'yyMMddHHmm'"') do set "BUILD_TIME=%%I"

REM ---- Git hash (8-char short) ---------------------------------
for /f "tokens=1" %%I in ('git -C "%ROOT_DIR%" log --oneline -1 2^>nul') do set "GIT_HASH=%%I"
if not defined GIT_HASH set "GIT_HASH=00000000"

REM ---- Full tag: APP-1.1.0.2605301230_alpha ------------------
set "FULL_TAG=%PRODUCT_NAME%-%VERSION_MAJOR%.%VERSION_MINOR%.%VERSION_PATCH%.%BUILD_TIME%_%BUILD_TYPE%"
set "APP_TARGET=%FULL_TAG%.exe"
set "TEST_TARGET=TEST.exe"

REM ============================================================
REM  Argument dispatch
REM ============================================================
set "ARG1=%~1"
if "%ARG1%"==""        set "BUILD_MODE=all"   & goto :dispatch_done
if /i "%ARG1%"=="all"   set "BUILD_MODE=all"   & goto :dispatch_done
if /i "%ARG1%"=="test"  set "BUILD_MODE=test"  & goto :dispatch_done
if /i "%ARG1%"=="shell" set "BUILD_MODE=shell" & set "AGENT_SHELL_ONLY=1" & goto :dispatch_done
if /i "%ARG1%"=="clean" set "BUILD_MODE=clean" & goto :dispatch_done
if /i "%ARG1%"=="clear" set "BUILD_MODE=clean" & goto :dispatch_done
if /i "%ARG1%"=="help"  set "BUILD_MODE=help"  & goto :dispatch_done
if /i "%ARG1%"=="-h"    set "BUILD_MODE=help"  & goto :dispatch_done
if /i "%ARG1%"=="--help" set "BUILD_MODE=help"  & goto :dispatch_done
if /i "%ARG1%"=="package" set "BUILD_MODE=package" & goto :do_package
echo [ERROR] Unknown argument: %ARG1%
echo Use 'build.bat help' for usage.
exit /b 1

:dispatch_done

REM ============================================================
REM  Mode: HELP
REM ============================================================
if not "%BUILD_MODE%"=="help" goto :skip_help
echo.
echo ============================================================
echo  Windows-C Build Framework
echo ============================================================
echo.
echo  USAGE:
echo    build.bat              Build everything (same as 'all')
echo    build.bat all          Build everything -^> %APP_TARGET%
echo    build.bat test         Build test only   -^> %TEST_TARGET%
echo    build.bat shell        Build shell-only  (AGENT_SHELL_ONLY=1, mock backend)
echo    build.bat clean        Remove all build artifacts (out/)
echo    build.bat clear        Same as clean
echo    build.bat package      Build Windows installer (Inno Setup) -^> dist\agent-setup-1.1.0.exe
echo    build.bat help         Show this help
echo.
echo  PRODUCT:
echo    Name:       %PRODUCT_NAME%
echo    Version:    %VERSION_MAJOR%.%VERSION_MINOR%.%VERSION_PATCH%
echo    Build type: %BUILD_TYPE%
echo    Full tag:   %FULL_TAG%
echo.
echo  TOOLS:
echo    MinGW:      %MINGW_PATH%
echo    MinGW zip:  %MINGW_ZIP%
echo    CMake:      %CMAKE_BIN%
echo    CMake zip:  %CMAKE_ZIP%
echo.
exit /b 0
:skip_help

REM ============================================================
REM  Mode: CLEAN
REM ============================================================
if not "%BUILD_MODE%"=="clean" goto :skip_clean
echo [CLEAN] Removing build output directory...
if not exist "%OUT_DIR%" goto :clean_no_out
rmdir /s /q "%OUT_DIR%"
echo [CLEAN] '%OUT_DIR%' removed.
goto :clean_done
:clean_no_out
echo [CLEAN] '%OUT_DIR%' does not exist, nothing to clean.
:clean_done
exit /b 0
:skip_clean

REM ============================================================
REM  Toolchain: check / extract
REM ============================================================
if not exist "%MINGW_PATH%\gcc.exe" (
    if not exist "%MINGW_ZIP%" goto :no_mingw_zip
    echo [SETUP] Extracting %MINGW_ZIP% ...
    powershell -Command "Expand-Archive -Path '%MINGW_ZIP%' -DestinationPath '%TOOLS_DIR%' -Force"
    if not exist "%MINGW_PATH%\gcc.exe" (
        echo [ERROR] Extraction failed: %MINGW_PATH%\gcc.exe
        exit /b 1
    )
    echo [SETUP] MinGW extracted.
)

if not exist "%CMAKE_BIN%\cmake.exe" (
    if not exist "%CMAKE_ZIP%" goto :no_cmake_zip
    echo [SETUP] Extracting %CMAKE_ZIP% ...
    powershell -Command "Expand-Archive -Path '%CMAKE_ZIP%' -DestinationPath '%TOOLS_DIR%' -Force"
    if not exist "%CMAKE_BIN%\cmake.exe" (
        echo [ERROR] Extraction failed: %CMAKE_BIN%\cmake.exe
        exit /b 1
    )
    echo [SETUP] CMake extracted.
)
goto :tools_ok

:no_mingw_zip
echo [ERROR] MinGW not found:
echo   gcc.exe: %MINGW_PATH%\gcc.exe
echo   Zip:     %MINGW_ZIP%
echo Please place the zip at the path above and re-run.
exit /b 1

:no_cmake_zip
echo [ERROR] CMake not found:
echo   cmake.exe: %CMAKE_BIN%\cmake.exe
echo   Zip:       %CMAKE_ZIP%
echo Please place the zip at the path above and re-run.
exit /b 1

:tools_ok

REM ============================================================
REM  Environment
REM ============================================================
set "PATH=%MINGW_PATH%;%CMAKE_BIN%;%PATH%"

REM ============================================================
REM  Build info
REM ============================================================
echo.
echo ============================================================
echo  Windows-C Build Framework
echo ============================================================
echo  Product : %FULL_TAG%
echo  Mode    : %BUILD_MODE%
echo  Time    : %BUILD_TIME%
echo  Git     : %GIT_HASH%
echo ============================================================
echo.
echo --- Toolchain versions ---
gcc   --version 2>nul | findstr /i "gcc"
cmake --version 2>nul | findstr /i "cmake"
echo ============================================================
echo.

REM ============================================================
REM  Prepare build dir
REM ============================================================
if not exist "%BUILD_DIR%" goto :mkdir_build
echo [CLEAN] Removing previous build directory...
rmdir /s /q "%BUILD_DIR%"
:mkdir_build
mkdir "%BUILD_DIR%" 2>nul

REM ============================================================
REM  CMake flags
REM ============================================================
set "CMAKE_FLAGS=-G "MinGW Makefiles""
set "CMAKE_FLAGS=%CMAKE_FLAGS% -DCMAKE_C_COMPILER=%MINGW_PATH%\gcc.exe"
set "CMAKE_FLAGS=%CMAKE_FLAGS% -DCMAKE_CXX_COMPILER=%MINGW_PATH%\g++.exe"
set "CMAKE_FLAGS=%CMAKE_FLAGS% -DCMAKE_MAKE_PROGRAM=%MINGW_PATH%\mingw32-make.exe"
set "CMAKE_FLAGS=%CMAKE_FLAGS% -DCMAKE_BUILD_TYPE=Release"
set "CMAKE_FLAGS=%CMAKE_FLAGS% -DPRODUCT_NAME=%PRODUCT_NAME%"
set "CMAKE_FLAGS=%CMAKE_FLAGS% -DPRODUCT_VERSION=%VERSION_MAJOR%.%VERSION_MINOR%.%VERSION_PATCH%"
set "CMAKE_FLAGS=%CMAKE_FLAGS% -DVERSION_MAJOR=%VERSION_MAJOR%"
set "CMAKE_FLAGS=%CMAKE_FLAGS% -DVERSION_MINOR=%VERSION_MINOR%"
set "CMAKE_FLAGS=%CMAKE_FLAGS% -DVERSION_PATCH=%VERSION_PATCH%"
set "CMAKE_FLAGS=%CMAKE_FLAGS% -DBUILD_TIME=%BUILD_TIME%"
set "CMAKE_FLAGS=%CMAKE_FLAGS% -DBUILD_TYPE=%BUILD_TYPE%"
set "CMAKE_FLAGS=%CMAKE_FLAGS% -DFULL_TAG=%FULL_TAG%"
set "CMAKE_FLAGS=%CMAKE_FLAGS% -DGIT_HASH=%GIT_HASH%"
set "CMAKE_FLAGS=%CMAKE_FLAGS% -DPROJ_OUT_DIR=%OUT_DIR%"

if not "%BUILD_MODE%"=="test" goto :flags_notest
set "CMAKE_FLAGS=%CMAKE_FLAGS% -DBUILD_TEST=ON"
goto :flags_done
:flags_notest
set "CMAKE_FLAGS=%CMAKE_FLAGS% -DBUILD_TEST=OFF"
:flags_done

REM ============================================================
REM  CMake Configure
REM ============================================================
echo [CMAKE] Configuring...
cd /d "%BUILD_DIR%"
cmake "%ROOT_DIR%" %CMAKE_FLAGS%
if not errorlevel 1 goto :cmake_config_ok
echo.
echo [ERROR] CMake configuration failed!
cd /d "%ROOT_DIR%"
exit /b 1
:cmake_config_ok

REM ============================================================
REM  Build
REM ============================================================
echo.
echo [BUILD] Compiling...
mingw32-make -j%NUMBER_OF_PROCESSORS%
if not errorlevel 1 goto :build_ok
echo.
echo [ERROR] Build failed!
cd /d "%ROOT_DIR%"
exit /b 1
:build_ok

REM ============================================================
REM  Done
REM ============================================================
cd /d "%ROOT_DIR%"

if not "%BUILD_MODE%"=="test" goto :done_app
if exist "%OUT_DIR%\TEST\%TEST_TARGET%" goto :print_test
goto :done_end
:done_app
if exist "%OUT_DIR%\%PRODUCT_NAME%\%APP_TARGET%" goto :print_app
goto :done_end

:print_test
echo.
echo ============================================================
echo  BUILD SUCCESS
echo  Output: %OUT_DIR%\TEST\%TEST_TARGET%
echo ============================================================
goto :done_end

:print_app
echo.
echo ============================================================
echo  BUILD SUCCESS
echo  Output: %OUT_DIR%\%PRODUCT_NAME%\%APP_TARGET%
echo ============================================================
goto :done_end

:done_end
echo.
endlocal
exit /b 0

REM ============================================================
REM  Mode: PACKAGE (Inno Setup)
REM ============================================================
:do_package
echo.
echo ============================================================
echo  Modem Agent Installer Build
echo ============================================================
echo  Version : %VERSION_MAJOR%.%VERSION_MINOR%.%VERSION_PATCH%
echo  Output  : dist\agent-setup-%VERSION_MAJOR%.%VERSION_MINOR%.%VERSION_PATCH%.exe
echo ============================================================
echo.

REM ---- 探测 iscc.exe（PATH 或 Inno Setup 标准安装位置） ---------
set "ISCC_EXE="
where iscc >nul 2>&1
if errorlevel 1 goto :iscc_not_in_path
set "ISCC_EXE=iscc"
goto :iscc_found
:iscc_not_in_path
if exist "C:\Program Files (x86)\Inno Setup 6\ISCC.exe" set "ISCC_EXE=C:\Program Files (x86)\Inno Setup 6\ISCC.exe"
if exist "C:\Program Files\Inno Setup 6\ISCC.exe" set "ISCC_EXE=C:\Program Files\Inno Setup 6\ISCC.exe"
:iscc_found

if "%ISCC_EXE%"=="" goto :iscc_missing

REM ---- 探测 build 产物（必须先 build 一次） ---------------------
if not exist "%OUT_DIR%\%PRODUCT_NAME%\%APP_TARGET%" goto :build_missing

REM ---- 准备 dist 目录 -------------------------------------------
if not exist "%ROOT_DIR%\dist" mkdir "%ROOT_DIR%\dist"

REM ---- 调 iscc 编译 --------------------------------------------
echo [ISCC] Compiling installer via %ISCC_EXE% ...
"%ISCC_EXE%" /DMyAppVersion=%VERSION_MAJOR%.%VERSION_MINOR%.%VERSION_PATCH% "%ROOT_DIR%\tools\installer\agent.iss"
if not errorlevel 1 goto :package_ok
echo.
echo [ERROR] Inno Setup compilation failed.
exit /b 1

:iscc_missing
echo [ERROR] Inno Setup 6 (iscc.exe) not found.
echo.
echo  Install Inno Setup 6 from: https://jrsoftware.org/isdl.php
echo.
echo  After install, either:
echo    1) Re-run 'build.bat package' (iscc on PATH)
echo    2) Or run manually:
echo       "C:\Program Files (x86)\Inno Setup 6\ISCC.exe" tools\installer\agent.iss
echo       /DMyAppVersion=%VERSION_MAJOR%.%VERSION_MINOR%.%VERSION_PATCH%
echo.
exit /b 1

:build_missing
echo [ERROR] Build output not found: %OUT_DIR%\%PRODUCT_NAME%\%APP_TARGET%
echo Run 'build.bat' first, then 'build.bat package'.
exit /b 1

:package_ok
echo.
echo ============================================================
echo  INSTALLER BUILD SUCCESS
echo  Output : dist\agent-setup-%VERSION_MAJOR%.%VERSION_MINOR%.%VERSION_PATCH%.exe
echo ============================================================
endlocal
exit /b 0
