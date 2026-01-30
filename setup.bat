@echo off
setlocal EnableExtensions

REM Always run from the repo root (directory containing this script)
set "ROOT_DIR=%~dp0"
pushd "%ROOT_DIR%" || exit /b 1

REM Optional: clone The Forge if requested.
if /I "%BZ3_SETUP_FORGE%"=="1" (
	if not exist "third_party\\the-forge\\Common_3" (
		echo [setup] The Forge not found; cloning...
		git clone https://github.com/ConfettiFX/The-Forge.git third_party\the-forge
		if errorlevel 1 (
			echo [setup] ERROR: failed to clone The Forge (is git installed?)
			popd
			exit /b 1
		)
	)
)

REM Download vcpkg if missing
if not exist "vcpkg\" (
	echo [setup] vcpkg\ not found; cloning...
	git clone https://github.com/microsoft/vcpkg.git
	if errorlevel 1 (
		echo [setup] ERROR: failed to clone vcpkg (is git installed?)
		popd
		exit /b 1
	)
)

REM Bootstrap vcpkg if needed
if not exist "vcpkg\vcpkg.exe" (
	echo [setup] Bootstrapping vcpkg...
	call "vcpkg\bootstrap-vcpkg.bat"
	if errorlevel 1 (
		echo [setup] ERROR: failed to bootstrap vcpkg
		popd
		exit /b 1
	)
)

REM Default to x64 on 64-bit machines (user can override via env)
if not defined VCPKG_DEFAULT_TRIPLET (
	if /I "%PROCESSOR_ARCHITECTURE%"=="AMD64" set "VCPKG_DEFAULT_TRIPLET=x64-windows"
)

echo [setup] Installing vcpkg dependencies (vcpkg.json)...
call "vcpkg\vcpkg.exe" install
if errorlevel 1 (
	echo [setup] ERROR: vcpkg install failed
	popd
	exit /b 1
)

echo [setup] Configuring CMake (Release) with vcpkg toolchain...
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_TOOLCHAIN_FILE="%ROOT_DIR%vcpkg\scripts\buildsystems\vcpkg.cmake"
if errorlevel 1 (
	echo [setup] ERROR: CMake configure failed
	popd
	exit /b 1
)

echo [setup] Setup complete. Build with: cmake --build build

popd
endlocal
