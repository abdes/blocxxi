# ===-----------------------------------------------------------------------===#
# Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
# copy at https://opensource.org/licenses/BSD-3-Clause.
# SPDX-License-Identifier: BSD-3-Clause
# ===-----------------------------------------------------------------------===#

import os
import json
from typing import Any, cast
from conan import ConanFile
from conan.tools.cmake import CMakeToolchain, CMakeDeps
from conan.tools.files import load, copy
from conan.tools.cmake import cmake_layout, CMake
from conan.tools.microsoft import is_msvc_static_runtime, is_msvc
from conan.errors import ConanInvalidConfiguration
from pathlib import Path


class BlocxxiConan(ConanFile):
    deploy_folder: str
    output: Any
    settings: Any
    cpp: Any
    conf: Any
    folders: Any
    dependencies: Any
    recipe_folder: Any

    name = "blocxxi"
    description = "Blocxxi re-founded on the nova reusable-module scaffold."
    license = "BSD 3-Clause License"
    homepage = "https://github.com/abdes/blocxxi"
    url = "https://github.com/abdes/blocxxi"
    topics = ("nova", "blockchain", "kademlia", "cmake", "conan")

    settings = "os", "arch", "compiler", "build_type", "sanitizer"
    options: Any = {
        "shared": [True, False],
        "fPIC": [True, False],
        "with_asan": [True, False],
        "with_coverage": [True, False],
        "examples": [True, False],
        "tests": [True, False],
        "docs": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
        "with_asan": False,
        "with_coverage": False,
        "examples": True,
        "tests": True,
        "docs": False,
        "fmt/*:header_only": True,
    }

    exports_sources = (
        "VERSION",
        "README.md",
        "LICENSE",
        "CMakeLists.txt",
        ".clangd.in",
        "cmake/**",
        "src/**",
        "tools/**",
        "!out/**",
        "!build/**",
        "!cmake-build-*/**",
    )

    def set_version(self):
        content = load(self, Path(self.recipe_folder) / "VERSION").strip()
        if content.startswith("\ufeff"):
            content = content[1:]
        self.version = content

    def requirements(self):
        self.requires("fmt/12.1.0")
        self.requires("ms-gsl/4.2.0")
        self.requires("asio/1.36.0")
        self.requires("cryptopp/8.9.0")
        self.requires("magic_enum/0.9.7")
        self.requires("miniupnpc/2.2.5")
        self._test_deps = set()
        if self.options.tests:
            ref = "gtest/master"
            self.test_requires(ref)
            self._test_deps.add(ref.split("/")[0])

    def configure(self):
        if self.options.shared:
            self.options.rm_safe("fPIC")
            if is_msvc(self) and is_msvc_static_runtime(self):
                self.output.error(
                    "Should not build shared libraries with static runtime!"
                )
                raise Exception("Invalid build configuration")

        try:
            self.options["gtest"].shared = False
        except Exception:
            pass

    def validate(self):
        if self._with_asan and self.settings.build_type != "Debug":
            raise ConanInvalidConfiguration(
                "ASan is only supported for Debug builds. "
                f"Current build_type is {self.settings.build_type}."
            )

    @property
    def _is_ninja(self):
        """Identify if Ninja (Multi-Config) is requested via conf or environment."""
        gen = self.conf.get("tools.cmake.cmaketoolchain:generator", default="")
        return "Ninja" in str(gen) or (not gen and "VSCODE_PID" in os.environ)

    @property
    def _with_asan(self):
        """Determine if ASAN is enabled via settings or options."""
        return self.settings.get_safe("sanitizer") == "asan" or bool(
            self.options.get_safe("with_asan")
        )

    @property
    def _install_subfolder(self):
        """Determine the subfolder for deployment or installation."""
        return "Asan" if self._with_asan else str(self.settings.build_type)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.absolute_paths = True
        tc.user_presets_path = (
            "ConanPresets-Ninja.json" if self._is_ninja else "ConanPresets-VS.json"
        )

        self._set_cmake_defs(tc.variables)
        if is_msvc(self):
            tc.variables["USE_MSVC_RUNTIME_LIBRARY_DLL"] = (
                not is_msvc_static_runtime(self)
            )

        install_base = str(Path(cast(str, self.recipe_folder)) / "out" / "install")
        tc.variables["NOVA_CONAN_DEPLOY_DIR"] = install_base.replace("\\", "/")
        tc.variables["NOVA_WITH_ASAN"] = "ON" if self._with_asan else "OFF"
        if self.options.with_coverage:
            tc.cache_variables["NOVA_WITH_COVERAGE"] = "ON"

        tc.generate()
        deps = CMakeDeps(self)
        deps.generate()

        if self._with_asan:
            try:
                self._append_asan_to_presets()
            except Exception as e:
                self.output.warning(f"Failed to append -asan presets: {e}")

    def _append_asan_to_presets(self):
        """Rename Conan-generated presets in ASAN builds to `-asan` variants."""
        build_dir = getattr(self.folders, "build", None)
        candidates = []
        if build_dir:
            candidates.append(Path(build_dir))

        suffix = "ninja" if self._is_ninja else "vs"
        if self._with_asan:
            suffix = f"asan-{suffix}"
        candidates.append(Path(self.recipe_folder) / f"out/build-{suffix}")

        presets_path = None
        for candidate in candidates:
            candidate_path = candidate / "generators" / "CMakePresets.json"
            self.output.info(f"Looking for CMakePresets at {candidate_path}")
            if candidate_path.exists():
                presets_path = candidate_path
                break

        if presets_path is None:
            self.output.info(
                "Presets not found in candidate locations; skipping -asan augmentation"
            )
            return

        with open(presets_path, "r", encoding="utf-8-sig") as f:
            data = json.load(f)

        name_map = {}
        for p in data.get("configurePresets", []):
            name = p.get("name")
            if not name or name.endswith("-asan"):
                continue
            new_name = name + "-asan"
            name_map[name] = new_name
            p["name"] = new_name
            if "displayName" in p:
                p["displayName"] = p["displayName"].replace(name, new_name)
            if "description" in p and "ASan" not in p["description"]:
                p["description"] = p["description"] + " (ASan)"
        for p in data.get("configurePresets", []):
            inherits = p.get("inherits")
            if inherits in name_map:
                p["inherits"] = name_map[inherits]
        for section in ("buildPresets", "testPresets"):
            for p in data.get(section, []):
                cfg = p.get("configurePreset")
                if cfg in name_map:
                    p["configurePreset"] = name_map[cfg]
                pname = p.get("name")
                if pname and not pname.endswith("-asan"):
                    p["name"] = pname + "-asan"

        with open(presets_path, "w", encoding="utf-8") as f:
            json.dump(data, f, indent=4)

        self.output.info(f"Renamed presets to '-asan' variants in {presets_path}")

    def layout(self):
        suffix = "ninja" if self._is_ninja else "vs"
        if self._with_asan:
            suffix = f"asan-{suffix}"
        cmake_layout(self, build_folder=f"out/build-{suffix}")

        self.cpp.build.includedirs.append(
            os.path.join(self.folders.build, "include")
        )

    def _set_cmake_defs(self, defs):
        defs["NOVA_BUILD_TESTS"] = self.options.tests
        defs["NOVA_BUILD_EXAMPLES"] = self.options.examples
        defs["NOVA_BUILD_DOCS"] = self.options.docs
        defs["BUILD_SHARED_LIBS"] = self.options.shared

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def test(self):
        cmake = CMake(self)
        cmake.test()

    def package(self):
        cmake = CMake(self)
        cmake.install()

    def deploy(self):
        test_deps = getattr(self, "_test_deps", set())
        target_deploy_folder = os.path.join(
            self.deploy_folder, self._install_subfolder
        )

        def try_copy(patterns, src, dst, pkg_name):
            for pattern in patterns:
                try:
                    copy(self, pattern, src=src, dst=dst)
                except Exception as e:
                    self.output.error(
                        f"Failed copying {pattern} from {pkg_name}: {e}"
                    )
                    raise

        for dep in self.dependencies.values():
            try:
                dep_name = dep.ref.name if dep.ref is not None else None
            except Exception:
                dep_name = None

            if dep_name in test_deps:
                continue

            if dep_name:
                name = dep_name
            else:
                try:
                    name = os.path.basename(dep.package_folder)
                except Exception:
                    name = "unknown"

            for incdir in getattr(dep.cpp_info, "includedirs", []) or []:
                try:
                    copy(
                        self,
                        "*",
                        src=incdir,
                        dst=os.path.join(target_deploy_folder, "include"),
                    )
                except Exception as e:
                    self.output.error(
                        f"Failed copying headers from {name}: {e}"
                    )
                    raise

            for libdir in getattr(dep.cpp_info, "libdirs", []) or []:
                try_copy(
                    ["*.lib", "*.a"],
                    libdir,
                    os.path.join(target_deploy_folder, "lib"),
                    name,
                )
                try_copy(
                    ["*.dll", "*.so*", "*.dylib*"],
                    libdir,
                    os.path.join(target_deploy_folder, "bin"),
                    name,
                )

            for bindir in getattr(dep.cpp_info, "bindirs", []) or []:
                try_copy(
                    ["*.exe", "*.dll", "*.so*", "*.dylib*"],
                    bindir,
                    os.path.join(target_deploy_folder, "bin"),
                    name,
                )
