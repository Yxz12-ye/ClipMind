# Repository Guidelines

## Project Structure & Module Organization
`src/` contains the application code, split by responsibility: `controller/` for coordination logic, `service/` for clipboard and storage services, and `ui/` for Qt widgets and windows. Keep new classes in paired `*.hpp`/`*.cpp` files near related code. `tests/` mirrors production modules with one CMake target per component, for example `tests/CopyEventListener/`. `assets/` stores Qt resources, `cmake/` holds shared CMake helpers, `3rdparty/` is for vendored dependencies, and `ui/UI图.pen` is a design artifact rather than runtime code. Build output belongs in `build/` and `out/`; do not commit generated files there.

## Build, Test, and Development Commands
Prefer CMake presets over ad hoc configure flags.

- **大部分情况下在更改后不要执行构建命令, 如果被要求构建请提权后构建而不是在沙箱里直接构建.**
- `cmake --preset win-msvc-ninja-debug` configures a Windows debug build.
- `cmake --build --preset win-msvc-ninja-debug` builds the `ClipMind` executable.
- `ctest --test-dir build/win-msvc-ninja-debug --output-on-failure` runs all discovered tests.
- `pwsh ./build.ps1 -BuildType Debug` uses the repository helper script to auto-pick a Windows preset.
- `cmake --preset linux-gcc-debug` is the matching Linux entry point when working off Windows.

## Coding Style & Naming Conventions
This project targets C++20 with Qt6. Run `clang-format -i` using the root `.clang-format` before submitting changes; it enforces 4-space indentation, 100-column lines, attached braces, and left-aligned pointers/references. Follow the existing naming pattern: PascalCase for classes (`UIController`), lowerCamelCase for methods and variables (`refreshCurrentView`), and descriptive test names using `*_test.cpp`. Keep includes grouped with standard/Qt headers first and project headers after them.

## Testing Guidelines
Tests use GoogleTest through CMake `gtest_discover_tests()`. Add new tests under `tests/<ModuleName>/` with a local `CMakeLists.txt`, then register that directory from `tests/CMakeLists.txt`. Cover both platform-neutral behavior and Windows-specific clipboard paths where relevant. Run `ctest` locally for the preset you changed before opening a PR.

## Commit & Pull Request Guidelines
Recent history follows short conventional prefixes such as `feat:`, `fix:`, and `docs:`; keep that format and make the summary specific, for example `fix: restore clipboard listener shutdown`. Pull requests should describe the user-visible change, note the preset and platform used for verification, link related issues, and include screenshots or GIFs for UI changes in `src/ui/`.
