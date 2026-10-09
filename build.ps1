# build.ps1 -- direct g++ build for this lab machine.
#
# This machine has GLFW and GLEW installed via MSYS2/MinGW but no CMake, so
# this script compiles the project directly with g++ from the MinGW64
# toolchain instead of going through CMakeLists.txt (which is still provided
# for a machine that does have CMake -- see CMakeLists.txt).
#
# Two toolchain gotchas this script works around (see IMPLEMENTATION_GUIDE.md
# Section 2 for the full story):
#   1. There is a second, older MinGW install on this machine
#      (C:\Program Files\CodeBlocks\MinGW) that silently crashes cc1plus.exe
#      if it comes before C:\msys64\mingw64\bin in PATH. We force mingw64\bin
#      to the front of PATH for this process only.
#   2. Dynamically linking the MinGW runtime DLLs means the built .exe would
#      depend on PATH resolving to the *matching* runtime at demo time, which
#      isn't guaranteed. We statically link the runtime so the .exe is fully
#      self-contained.
$ErrorActionPreference = "Stop"

$MINGW = "C:\msys64\mingw64"
$env:PATH = "$MINGW\bin;" + $env:PATH
$GXX = "$MINGW\bin\g++.exe"

$sources = @(
    "src/main.cpp",
    "src/Shader.cpp",
    "src/Geometry.cpp",
    "src/SceneObjects.cpp",
    "src/NewObjects.cpp",
    "src/Environment.cpp",
    "src/Texture.cpp",
    "src/Effects.cpp"
)

New-Item -ItemType Directory -Force -Path "build" | Out-Null

$argsList = @(
    "-std=c++17", "-O2", "-DGLEW_STATIC",
    "-I$MINGW\include", "-Isrc"
) + $sources + @(
    "-L$MINGW\lib",
    "-static", "-static-libgcc", "-static-libstdc++",
    "-lglew32", "-lglfw3", "-lopengl32", "-lgdi32",
    "-o", "build/VolcanoX.exe"
)

Write-Host "Building VolcanoX..."
& $GXX $argsList
if ($LASTEXITCODE -ne 0) {
    Write-Host "Build failed." -ForegroundColor Red
    exit 1
}

# Shaders are loaded from disk at runtime relative to the working directory.
# Mirror them next to the executable so `build/VolcanoX.exe` also works when
# launched with `build` as the current directory.
New-Item -ItemType Directory -Force -Path "build/shaders" | Out-Null
Copy-Item -Path "shaders/*" -Destination "build/shaders" -Recurse -Force

Write-Host "Build succeeded: build/VolcanoX.exe" -ForegroundColor Green
Write-Host "Run it from the project root: ./build/VolcanoX.exe" -ForegroundColor Green
