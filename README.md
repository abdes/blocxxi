# Blocxxi — Nova-based blockchain and Kademlia stack

Blocxxi is being re-founded on Nova’s Conan + CMake scaffold while preserving
its blockchain, Kademlia, and reusable networking/domain code. The repository
now exposes a reusable blockchain platform surface (`Core`, `Chain`, `Storage`,
`Node`, `Bitcoin`) alongside the existing `P2P` subsystem, with project-owned
Blocxxi integration work layered on top of the Nova reusable modules under
`src/Nova/`.

---

## Table of Contents

- [Blocxxi — Nova-based blockchain and Kademlia stack](#blocxxi--nova-based-blockchain-and-kademlia-stack)
  - [Table of Contents](#table-of-contents)
  - [Prerequisites](#prerequisites)
  - [Repository Layout](#repository-layout)
  - [One-Time Environment Setup](#one-time-environment-setup)
    - [Visual Studio](#visual-studio)
    - [Python virtual environment](#python-virtual-environment)
    - [Pre-commit hooks](#pre-commit-hooks)
    - [Conan](#conan)
  - [Building](#building)
    - [Recommended: generate-builds script](#recommended-generate-builds-script)
    - [Manual Conan + CMake](#manual-conan--cmake)
  - [CMake Options](#cmake-options)
  - [Sanitizer Support (ASan)](#sanitizer-support-asan)
  - [License](#license)

---

## Prerequisites

| Tool          | Minimum version  | Notes                                     |
| ------------- | ---------------- | ----------------------------------------- |
| CMake         | 3.29             | Must be on `PATH`                         |
| Visual Studio | 2026 (MSVC 19.5) | **Desktop development with C++** workload |
| Python        | 3.10+            | For Conan and pre-commit                  |
| Conan         | 2.x              | Installed via `pip`                       |
| Git           | any recent       | Required for revision stamping            |

---

## Repository Layout

```text
├── CMakeLists.txt          # Top-level CMake project
├── CMakePresets.json       # Includes platform presets
├── conanfile.py            # Conan 2 recipe
├── conan-settings_user.yml # Extended Conan settings (sanitizer dimension)
├── VERSION                 # Semver source of truth (read by CMake & Conan)
├── profiles/
│   ├── windows-msvc.ini        # MSVC release profile
│   └── windows-msvc-asan.ini   # MSVC + ASan profile
├── cmake/                  # CMake helper modules
├── src/
│   ├── Nova/
│   │   ├── Base/           # Upstream Nova reusable modules
│   │   └── Testing/
│   └── Blocxxi/
│       ├── Codec/
│       ├── Crypto/
│       ├── Nat/
│       ├── Core/
│       ├── Chain/
│       ├── Storage/
│       ├── Node/
│       ├── Bitcoin/
│       ├── P2P/
│       └── Examples/
└── tools/
    ├── generate-builds.ps1 # One-shot Conan + CMake bootstrap script
    └── presets/            # CMake preset fragments
```

---

## One-Time Environment Setup

### Visual Studio

Install **Visual Studio 2026** with the **Desktop development with C++**
workload. Verify `vcvarsall.bat` is present:

```text
C:\Program Files\Microsoft Visual Studio\2026\Community\VC\Auxiliary\Build\vcvarsall.bat
```

### Python virtual environment

```powershell
python -m venv .venv
.\.venv\Scripts\Activate.ps1
```

### Pre-commit hooks

With the virtual environment active:

```powershell
.\init.ps1
```

### Conan

Install Conan, point it at your local fork of `conan-center-index`, and
register the custom user settings that add a `sanitizer` dimension to package
IDs.

Because the starter uses `gtest/master` for tests, the standard `conancenter`
remote is not sufficient. Keep your own fork of `conan-center-index` checked
out locally at `./conan-center-index`, then replace the default remote before
running any `conan install` commands or `tools/generate-builds.ps1`:

```powershell
pip install conan

conan remote remove conancenter
conan remote add mycenter ./conan-center-index

# Install the repo-local user settings into Conan home
$tmp = Join-Path $env:TEMP "settings_user.yml"
Copy-Item "conan-settings_user.yml" -Destination $tmp -Force
conan config install $tmp
```

Conan merges `settings_user.yml` from Conan home with its built-in settings at
runtime. This makes `sanitizer` a first-class setting that participates in
package IDs, preventing accidental mixing of ASan and non-ASan binaries in the
cache.

---

## Building

### Recommended: generate-builds script

[`tools/generate-builds.ps1`](tools/generate-builds.ps1) runs all Conan
installs and configures both a **Ninja Multi-Config** and a **Visual Studio**
build tree in one step. It accepts a single positional argument — the Conan
host profile — resolved relative to the repository root.

```powershell
# Standard build (Debug / Release / RelWithDebInfo)
.\tools\generate-builds.ps1 profiles/windows-msvc.ini

# ASan build (Debug only)
.\tools\generate-builds.ps1 profiles/windows-msvc-asan.ini

# Show all options
.\tools\generate-builds.ps1 -Help
```

Generated trees land in `out/`:

| Directory               | Generator                 |
| ----------------------- | ------------------------- |
| `out/build-ninja/`      | Ninja Multi-Config        |
| `out/build-vs/`         | Visual Studio 2026        |
| `out/build-asan-ninja/` | Ninja Multi-Config + ASan |
| `out/build-asan-vs/`    | Visual Studio 2026 + ASan |

Build with CMake after generation:

```powershell
cmake --build out/build-ninja --config Debug
cmake --build out/build-ninja --config Release
```

### Manual Conan + CMake

If you need finer control, run Conan and CMake directly.

> **Important:** The preset workflow expects **two Conan installs**:
>
> 1. one with the **normal** profile
> 2. one with the matching **`-asan`** profile
>
> Do this for the platform you are using:
> - Linux: `profiles/linux-clang.ini` + `profiles/linux-clang-asan.ini`
> - Windows: `profiles/windows-msvc.ini` + `profiles/windows-msvc-asan.ini`
>
> The normal and ASan trees are separate. Do **not** try to replace one with the
> other.

```powershell
# 1. Normal install
conan install . `
  --profile:host=profiles/windows-msvc.ini `
  --profile:build=profiles/windows-msvc.ini `
  --build=missing `
  -c tools.cmake.cmaketoolchain:generator="Ninja Multi-Config" `
  -s build_type=Debug

# 2. ASan install
conan install . `
  --profile:host=profiles/windows-msvc-asan.ini `
  --profile:build=profiles/windows-msvc-asan.ini `
  --build=missing `
  -c tools.cmake.cmaketoolchain:generator="Ninja Multi-Config" `
  -s build_type=Debug

# 3. Configure with CMake
cmake --preset windows-default

# 4. Build normal Debug / Release
cmake --build out/build-ninja --config Debug
cmake --build out/build-ninja --config Release

# 5. Build ASan Debug
cmake --preset windows-asan
cmake --build out/build-asan-ninja --config Debug
```

Linux uses the same two-install pattern:

```bash
# 1. Normal install
conan install . \
  --profile:host=profiles/linux-clang.ini \
  --profile:build=profiles/linux-clang.ini \
  --build=missing

# 2. ASan install
conan install . \
  --profile:host=profiles/linux-clang-asan.ini \
  --profile:build=profiles/linux-clang-asan.ini \
  --build=missing

# 3. Configure and build
cmake --preset linux
cmake --build out/build-ninja --config Debug
cmake --build out/build-ninja --config Release

cmake --preset linux-asan
cmake --build out/build-asan-ninja --config Debug
```

---

## CMake Options

These cache variables can be passed via `-D` or set in a CMake preset:

| Option                | Default | Description                              |
| --------------------- | ------- | ---------------------------------------- |
| `BUILD_SHARED_LIBS`   | `OFF`   | Build shared instead of static libraries |
| `NOVA_BUILD_TESTS`    | `ON`    | Build and register test targets          |
| `NOVA_BUILD_EXAMPLES` | `ON`    | Build example targets                    |
| `NOVA_BUILD_DOCS`     | `ON`    | Build documentation targets              |
| `NOVA_WITH_ASAN`      | `OFF`   | Instrument with Address Sanitizer        |
| `NOVA_WITH_COVERAGE`  | `OFF`   | Instrument for code coverage             |
| `NOVA_WITH_DOXYGEN`   | `OFF`   | Generate Doxygen API docs                |
| `NOVA_USE_CCACHE`     | `OFF`   | Cache compiled artifacts with ccache     |

> **Note:** When renaming the project, replace the `NOVA_` prefix with your
> project's prefix throughout `CMakeLists.txt` and `conanfile.py`.

Public-network validation
-------------------------

For reproducible public-network evidence, run:

```bash
python tools/public_network_validation.py
```

This validates:
- public Mainline DHT discovery via `mainline-node`
- public signet peer version/verack handshake
- public signet `getheaders`/`headers` exchange

---

## Sanitizer Support (ASan)

The starter extends Conan's built-in settings with a `sanitizer` dimension so
that ASan and non-ASan builds produce distinct package IDs and never collide in
the cache.

**How it works:**

1. [`conan-settings_user.yml`](conan-settings_user.yml) defines
   `sanitizer: [None, asan]`.
2. The `*-asan` profiles (for example
   [`profiles/windows-msvc-asan.ini`](profiles/windows-msvc-asan.ini) or
   `profiles/linux-clang-asan.ini`) set `sanitizer=asan`; the standard profiles
   set `sanitizer=None`.
3. [`conanfile.py`](conanfile.py) reads the setting inside `generate()` and
   sets `NOVA_WITH_ASAN=ON` and injects `-fsanitize=address` into the CMake
   toolchain.

To use the repo presets correctly, run **both** Conan installs:

- one with the normal profile
- one with the matching `-asan` profile

That produces two distinct configure/build trees:

- normal: `out/build-ninja/` or `out/build-vs/`
- ASan: `out/build-asan-ninja/` or `out/build-asan-vs/`

Debug tests must run against the **Debug** configuration, and Release tests
must run against the **Release** configuration of the normal multi-config tree.
ASan testing is a separate Debug-only tree.

---

## License

Distributed under the **3-Clause BSD License**.
See [LICENSE](LICENSE) for the full text.
