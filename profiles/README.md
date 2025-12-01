# Conan Profile Usage

Profiles allow you to use different compilers and generators with isolated build directories.

## Visual Studio Profiles

Build with Visual Studio 17 2022 (multi-config generator - supports Debug/Release in same build):

```powershell
# Install dependencies (one command supports all configurations)
conan install . --profile=profiles/vs-debug --output-folder=build/vs --build=missing

# Configure CMake
cmake --preset conan-default

# Build Debug or Release
cmake --build build/vs --config Debug
cmake --build build/vs --config Release
```

**Add to CMakeUserPresets.json:**
```json
"include": [
    "build/vs/generators/CMakePresets.json"
]
```

## Ninja Profiles

Build with Ninja (single-config generator - requires separate build directories):

```powershell
# Debug build
conan install . --profile=profiles/ninja-debug --output-folder=build/ninja-debug --build=missing
cmake --preset conan-default
cmake --build build/ninja-debug

# Release build
conan install . --profile=profiles/ninja-release --output-folder=build/ninja-release --build=missing
cmake --preset conan-default
cmake --build build/ninja-release
```

**Add to CMakeUserPresets.json:**
```json
"include": [
    "build/ninja-debug/generators/CMakePresets.json",
    "build/ninja-release/generators/CMakePresets.json"
]
```

## Notes

- **Visual Studio**: Multi-config generator, supports Debug/Release/MinSizeRel/RelWithDebInfo in same build folder
- **Ninja**: Single-config generator, requires separate build folders per configuration
- **Ninja**: Generally faster build times, better for CI/CD
- **Visual Studio**: Better IDE integration, easier debugging in Visual Studio
- **Output folders**: Use `build/{profile-name}/` pattern (e.g., `build/vs/`, `build/ninja-debug/`)
- **CMakeUserPresets.json**: Must be manually updated when adding new build directories

## Quick Setup

For Ninja builds, ensure Ninja is installed:
```powershell
choco install ninja
# or
winget install Ninja-build.Ninja
```
