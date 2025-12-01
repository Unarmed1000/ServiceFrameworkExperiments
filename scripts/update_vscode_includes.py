#!/usr/bin/env python3
"""
Cross-platform VS Code IntelliSense configuration updater for Conan packages.
Automatically generates c_cpp_properties.json when Conan packages change.
"""

import argparse
import glob
import hashlib
import json
import os
import platform
import re
import shutil
import subprocess
import sys
from pathlib import Path
from typing import Dict, List, Optional, Set, Tuple


class CMakeToolsValidator:
    """Validates and caches CMake Tools extension availability."""

    CACHE_FILE = ".cmake_tools_validated"

    @staticmethod
    def is_cmake_tools_installed(vscode_dir: Path) -> bool:
        """Check if CMake Tools extension is installed by looking for workspace state."""
        cache_file = vscode_dir / CMakeToolsValidator.CACHE_FILE

        # Check cache first
        if cache_file.exists():
            return True

        # Look for CMake Tools indicators
        # Extension creates these files when active
        indicators = [
            vscode_dir / "cmake-tools-kits.json",
            vscode_dir / ".cmaketools.json",
        ]

        for indicator in indicators:
            if indicator.exists():
                # Cache the validation
                cache_file.touch()
                return True

        # If no indicators found, assume CMake Tools will be installed/activated
        # Cache validation to avoid repeated warnings
        cache_file.touch()
        print(
            "Note: CMake Tools extension indicators not found. Extension is recommended.",
            file=sys.stderr,
        )
        print(
            "  Install from: https://marketplace.visualstudio.com/items?itemName=ms-vscode.cmake-tools",
            file=sys.stderr,
        )
        return True


class CompilerDetector:
    """Detects compilers for different platforms."""

    @staticmethod
    def detect_compiler() -> Tuple[Optional[str], str]:
        """
        Detect compiler path and IntelliSense mode.
        Returns: (compiler_path, intellisense_mode)
        """
        system = platform.system()

        if system == "Windows":
            return CompilerDetector._detect_msvc()
        elif system == "Darwin":
            return CompilerDetector._detect_macos_clang()
        elif system == "Linux":
            return CompilerDetector._detect_linux_compiler()
        else:
            return None, "gcc-x64"

    @staticmethod
    def _detect_msvc() -> Tuple[Optional[str], str]:
        """Detect MSVC on Windows using vswhere."""
        try:
            vswhere_path = (
                Path(os.environ.get("ProgramFiles(x86)", "C:\\Program Files (x86)"))
                / "Microsoft Visual Studio"
                / "Installer"
                / "vswhere.exe"
            )

            if vswhere_path.exists():
                result = subprocess.run(
                    [str(vswhere_path), "-latest", "-property", "installationPath"],
                    capture_output=True,
                    text=True,
                    check=True,
                )
                vs_path = result.stdout.strip()

                # Look for cl.exe in the VS installation
                cl_paths = list(
                    Path(vs_path).glob("VC/Tools/MSVC/*/bin/Hostx64/x64/cl.exe")
                )
                if cl_paths:
                    return str(cl_paths[0]), "windows-msvc-x64"
        except:
            pass

        # Fallback: try to find cl.exe in PATH
        cl_exe = shutil.which("cl")
        if cl_exe:
            return cl_exe, "windows-msvc-x64"

        return None, "windows-msvc-x64"

    @staticmethod
    def _detect_macos_clang() -> Tuple[Optional[str], str]:
        """Detect Clang on macOS using xcrun."""
        try:
            result = subprocess.run(
                ["xcrun", "--find", "clang"], capture_output=True, text=True, check=True
            )
            return result.stdout.strip(), "macos-clang-x64"
        except:
            pass

        # Fallback to which
        clang_path = shutil.which("clang")
        return clang_path, "macos-clang-x64"

    @staticmethod
    def _detect_linux_compiler() -> Tuple[Optional[str], str]:
        """Detect GCC or Clang on Linux."""
        # Prefer GCC
        gcc_path = shutil.which("gcc")
        if gcc_path:
            return gcc_path, "linux-gcc-x64"

        # Fallback to Clang
        clang_path = shutil.which("clang")
        if clang_path:
            return clang_path, "linux-clang-x64"

        return None, "linux-gcc-x64"


class ConanDataParser:
    """Parses Conan CMake data files to extract package information."""

    @staticmethod
    def parse_data_file(file_path: Path) -> Dict[str, any]:
        """Parse a Conan *-data.cmake file and extract relevant information."""
        data = {
            "include_dirs": [],
            "defines": [],
            "framework_dirs": [],
            "frameworks": [],
        }

        content = file_path.read_text()

        # Extract include directories
        match = re.search(r'set\(\w+_INCLUDE_DIRS_\w+ "([^"]+)"\)', content)
        if match:
            data["include_dirs"].append(
                match.group(1).replace("${", "").replace("}", "")
            )

        # Extract package folder for path construction
        pkg_folder_match = re.search(
            r'set\((\w+)_PACKAGE_FOLDER_\w+ "([^"]+)"\)', content
        )
        if pkg_folder_match:
            pkg_folder = pkg_folder_match.group(2)
            # Re-extract include dirs with resolved path
            include_match = re.search(
                r'set\(\w+_INCLUDE_DIRS_\w+ "\$\{.*?PACKAGE_FOLDER.*?\}/([^"]+)"\)',
                content,
            )
            if include_match:
                data["include_dirs"] = [
                    os.path.join(pkg_folder, include_match.group(1))
                ]
            elif not data["include_dirs"]:
                data["include_dirs"] = [os.path.join(pkg_folder, "include")]

        # Extract defines
        defines_match = re.search(
            r"set\(\w+_COMPILE_DEFINITIONS_\w+ ([^)]+)\)", content
        )
        if defines_match:
            defines_str = defines_match.group(1).strip('"')
            if defines_str:
                data["defines"].extend(defines_str.split())

        # Extract framework directories (macOS)
        framework_dirs_match = re.search(
            r"set\(\w+_FRAMEWORK_DIRS_\w+ ([^)]+)\)", content
        )
        if framework_dirs_match:
            dirs_str = framework_dirs_match.group(1).strip('"')
            if dirs_str:
                data["framework_dirs"].extend(dirs_str.split(";"))

        # Extract frameworks (macOS)
        frameworks_match = re.search(r"set\(\w+_FRAMEWORKS_\w+ ([^)]+)\)", content)
        if frameworks_match:
            frameworks_str = frameworks_match.group(1).strip('"')
            if frameworks_str:
                data["frameworks"].extend(frameworks_str.split(";"))

        return data

    @staticmethod
    def scan_generators_directory(
        generators_dir: Path, build_type: str
    ) -> Dict[str, any]:
        """Scan generators directory for all Conan package data files."""
        result = {"include_dirs": set(), "defines": set(), "framework_dirs": set()}

        # Find all *-{buildtype}-{arch}-data.cmake files
        pattern = f"*-{build_type.lower()}-*-data.cmake"
        data_files = list(generators_dir.glob(pattern))

        for data_file in data_files:
            data = ConanDataParser.parse_data_file(data_file)
            result["include_dirs"].update(data["include_dirs"])
            result["defines"].update(data["defines"])
            result["framework_dirs"].update(data["framework_dirs"])

        return result


class CMakeCacheReader:
    """Reads information from CMakeCache.txt."""

    @staticmethod
    def read_cache(cache_file: Path) -> Dict[str, str]:
        """Read CMakeCache.txt and extract key variables."""
        cache_data = {}

        if not cache_file.exists():
            return cache_data

        content = cache_file.read_text()

        # Extract variables
        patterns = {
            "CMAKE_CXX_COMPILER": r"CMAKE_CXX_COMPILER:FILEPATH=(.+)",
            "CMAKE_BUILD_TYPE": r"CMAKE_BUILD_TYPE:STRING=(.+)",
            "CMAKE_CONFIGURATION_TYPES": r"CMAKE_CONFIGURATION_TYPES:STRING=(.+)",
            "CMAKE_GENERATOR": r"CMAKE_GENERATOR:INTERNAL=(.+)",
        }

        for key, pattern in patterns.items():
            match = re.search(pattern, content)
            if match:
                cache_data[key] = match.group(1).strip()

        return cache_data


class IntelliSenseConfigGenerator:
    """Generates VS Code c_cpp_properties.json configuration."""

    def __init__(self, workspace_root: Path):
        self.workspace_root = workspace_root
        self.vscode_dir = workspace_root / ".vscode"
        self.vscode_dir.mkdir(exist_ok=True)

    def generate_configuration(self, build_dirs: List[Path]) -> int:
        """Generate c_cpp_properties.json for all discovered build directories.

        Returns:
            0 = success with changes
            1 = error
            2 = success but no changes (all hashes matched)
        """
        configurations = []
        has_changes = False

        for build_dir in build_dirs:
            configs, changed = self._process_build_directory(build_dir)
            configurations.extend(configs)
            if changed:
                has_changes = True

        if not configurations:
            print(
                "Warning: No build configurations found. Run CMake configure first.",
                file=sys.stderr,
            )
            return 1

        # Only write file if there were changes
        if has_changes:
            # Create c_cpp_properties.json
            cpp_properties = {
                "version": 4,
                "configurations": configurations,
                "$comment": "CMake Tools extension provides primary IntelliSense via compile_commands.json. Explicit paths serve as fallback.",
            }

            output_file = self.vscode_dir / "c_cpp_properties.json"
            with open(output_file, "w") as f:
                json.dump(cpp_properties, f, indent=4)

            print(
                f"[IntelliSense] Updated {output_file} with {len(configurations)} configuration(s)"
            )
            return 0
        else:
            return 2

    def _process_build_directory(self, build_dir: Path) -> Tuple[List[Dict], bool]:
        """Process a single build directory and return configurations with change flag.

        Returns:
            (configurations, has_changes): Tuple of config list and whether any changes were detected
        """
        cache_file = build_dir / "CMakeCache.txt"
        if not cache_file.exists():
            return [], False

        cache_data = CMakeCacheReader.read_cache(cache_file)

        # Determine preset name from build directory
        preset_name = self._extract_preset_name(build_dir)

        # Check if multi-config generator
        config_types = cache_data.get("CMAKE_CONFIGURATION_TYPES", "").split(";")
        is_multi_config = len(config_types) > 1

        configurations = []
        has_changes = False
        valid_build_types = set()

        if is_multi_config:
            # Track valid build types for cleanup
            valid_build_types.update(config_types)
            # Generate configuration for each build type
            for build_type in config_types:
                if build_type:
                    config, changed = self._create_configuration(
                        build_dir, preset_name, build_type, cache_data
                    )
                    if config:
                        configurations.append(config)
                        if changed:
                            has_changes = True

            # Cleanup orphaned hash files
            self._cleanup_orphaned_hashes(preset_name, valid_build_types)
        else:
            # Single configuration
            build_type = cache_data.get("CMAKE_BUILD_TYPE", "Debug")
            valid_build_types.add(build_type)
            config, changed = self._create_configuration(
                build_dir, preset_name, build_type, cache_data
            )
            if config:
                configurations.append(config)
                if changed:
                    has_changes = True

            # Cleanup orphaned hash files
            self._cleanup_orphaned_hashes(preset_name, valid_build_types)

        return configurations, has_changes

    def _extract_preset_name(self, build_dir: Path) -> str:
        """Extract preset name from build directory path (build/{preset}/build/)."""
        # Traverse up to find preset name
        # Path structure: workspace/build/{preset}/build/
        try:
            parts = build_dir.parts
            # Find 'build' in path parts
            for i, part in enumerate(parts):
                if part == "build" and i + 2 < len(parts):
                    # Check if next part is preset and following is 'build'
                    if parts[i + 2] == "build":
                        return parts[i + 1]
        except Exception:
            pass

        # Fallback to parent directory name (preset directory)
        if build_dir.name == "build" and build_dir.parent.name != "build":
            return build_dir.parent.name

        # Final fallback
        return "default"

    def _create_configuration(
        self,
        build_dir: Path,
        preset_name: str,
        build_type: str,
        cache_data: Dict[str, str],
    ) -> Tuple[Optional[Dict], bool]:
        """Create a single IntelliSense configuration.

        Returns:
            (config, has_changes): Configuration dict (or None if skipped) and change flag
        """
        generators_dir = build_dir / "generators"
        if not generators_dir.exists():
            return None, False

        # Hash the generators directory content for change detection
        content_hash = self._hash_directory(generators_dir, build_type)
        hash_file = self.vscode_dir / f".conan_hash_{preset_name}_{build_type}"

        # Check if we need to regenerate
        has_changes = True
        if hash_file.exists():
            cached_hash = hash_file.read_text().strip()
            if cached_hash == content_hash:
                # No changes, but still return existing config structure
                has_changes = False

        # Parse Conan data (even if no changes, for consistency)
        conan_data = ConanDataParser.scan_generators_directory(
            generators_dir, build_type
        )

        # Detect compiler
        detected_compiler, intellisense_mode = CompilerDetector.detect_compiler()
        compiler_path = cache_data.get("CMAKE_CXX_COMPILER") or detected_compiler

        # Build configuration
        config = {
            "name": f"{preset_name}-{build_type}",
            "configurationProvider": "ms-vscode.cmake-tools",
            "includePath": self._build_include_path(conan_data),
            "defines": self._build_defines(build_type, conan_data),
            "compilerPath": compiler_path if compiler_path else "",
            "cStandard": "c17",
            "cppStandard": "c++20",
            "intelliSenseMode": intellisense_mode,
        }

        # Add macOS-specific framework paths
        if platform.system() == "Darwin" and conan_data["framework_dirs"]:
            config["macFrameworkPath"] = sorted(conan_data["framework_dirs"])

        # Store hash only if there were changes
        if has_changes:
            hash_file.write_text(content_hash)

        return config, has_changes

    def _build_include_path(self, conan_data: Dict) -> List[str]:
        """Build include path list."""
        paths = [
            "${workspaceFolder}/**",
            "${workspaceFolder}/include",
            "${workspaceFolder}/src",
        ]

        # Add Conan package includes
        paths.extend(sorted(conan_data["include_dirs"]))

        return paths

    def _build_defines(self, build_type: str, conan_data: Dict) -> List[str]:
        """Build defines list."""
        defines = ["UNICODE", "_UNICODE"]

        # Add build type specific defines
        if build_type == "Debug":
            defines.append("_DEBUG")
        else:
            defines.append("NDEBUG")

        # Add Conan package defines
        defines.extend(sorted(conan_data["defines"]))

        return defines

    def _extract_preset_name(self, build_dir: Path) -> str:
        """Extract preset name from build directory path (build/{preset}/build/)."""
        # Traverse up to find preset name
        # Path structure: workspace/build/{preset}/build/
        try:
            parts = build_dir.parts
            # Find 'build' in path parts
            for i, part in enumerate(parts):
                if part == "build" and i + 2 < len(parts):
                    # Check if next part is preset and following is 'build'
                    if parts[i + 2] == "build":
                        return parts[i + 1]
        except Exception:
            pass

        # Fallback to directory name
        return build_dir.parent.name if build_dir.name == "build" else build_dir.name

    def _hash_directory(self, directory: Path, build_type: str) -> str:
        """Hash all relevant data files in the generators directory."""
        hasher = hashlib.sha256()

        # Find all *-{buildtype}-*-data.cmake files
        pattern = f"*-{build_type.lower()}-*-data.cmake"
        data_files = sorted(directory.glob(pattern))

        for file_path in data_files:
            hasher.update(file_path.read_bytes())

        return hasher.hexdigest()

    def _cleanup_orphaned_hashes(
        self, preset_name: str, valid_build_types: Set[str]
    ) -> None:
        """Remove hash files for build types that no longer exist."""
        hash_prefix = f".conan_hash_{preset_name}_"

        for hash_file in self.vscode_dir.glob(f"{hash_prefix}*"):
            # Extract build type from hash filename
            build_type = hash_file.name[len(hash_prefix) :]
            if build_type not in valid_build_types:
                try:
                    hash_file.unlink()
                    print(f"[IntelliSense] Cleaned up orphaned hash: {hash_file.name}")
                except Exception as e:
                    print(
                        f"Warning: Could not remove {hash_file.name}: {e}",
                        file=sys.stderr,
                    )


def discover_build_directories(workspace_root: Path) -> List[Path]:
    """Discover all build directories with CMakeCache.txt in build/*/build/ pattern."""
    build_dirs = []

    # Scan for Conan-style build/{preset}/build/ directories
    build_root = workspace_root / "build"
    if build_root.exists() and build_root.is_dir():
        for preset_dir in build_root.iterdir():
            if preset_dir.is_dir():
                # Check for inner build/ directory (Conan pattern)
                inner_build = preset_dir / "build"
                if inner_build.is_dir() and (inner_build / "CMakeCache.txt").exists():
                    build_dirs.append(inner_build)

    return build_dirs


def main():
    parser = argparse.ArgumentParser(
        description="Update VS Code IntelliSense configuration from Conan packages"
    )
    parser.add_argument(
        "--workspace-root",
        type=Path,
        help="Workspace root directory (default: current directory)",
    )
    parser.add_argument(
        "--build-dir", type=Path, help="Specific build directory to process"
    )

    args = parser.parse_args()

    workspace_root = args.workspace_root or Path.cwd()
    vscode_dir = workspace_root / ".vscode"
    vscode_dir.mkdir(exist_ok=True)

    # Validate CMake Tools extension
    if not CMakeToolsValidator.is_cmake_tools_installed(vscode_dir):
        sys.exit(1)

    # Discover build directories
    if args.build_dir:
        build_dirs = [args.build_dir] if args.build_dir.exists() else []
    else:
        build_dirs = discover_build_directories(workspace_root)

    if not build_dirs:
        print("No build directories found. Run CMake configure first.", file=sys.stderr)
        sys.exit(1)

    # Generate configuration
    generator = IntelliSenseConfigGenerator(workspace_root)
    exit_code = generator.generate_configuration(build_dirs)

    if exit_code == 0:
        print("[IntelliSense] Configuration updated successfully")
    elif exit_code == 2:
        # No changes detected, silent success
        pass

    sys.exit(exit_code)


if __name__ == "__main__":
    main()
