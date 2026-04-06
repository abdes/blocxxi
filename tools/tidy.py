#!/usr/bin/env python3
"""Run clang-tidy for a module, a single file, or the repo source tree.

Examples:
  python tools/tidy.py
  python tools/tidy.py --module Base
  python tools/tidy.py --file src/Nova/Base/Hash.h
  python tools/tidy.py --module Base --exclude **/loguru.cpp
"""

from __future__ import annotations

import argparse
import platform
import re
import shlex
import subprocess
import sys
import tempfile
from concurrent.futures import ThreadPoolExecutor, as_completed
from dataclasses import dataclass
from pathlib import Path, PurePosixPath
from typing import Iterable


SOURCE_SUFFIXES = {".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx"}
DEFAULT_BUILD_DIR_CANDIDATES = (
    Path("out/build-ninja"),
    Path("out/build-asan-ninja"),
)


@dataclass(frozen=True)
class TidyResult:
    file: Path
    returncode: int
    output: str


def repo_root() -> Path:
    return Path(__file__).resolve().parent.parent


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Run clang-tidy from the repo root against src/Blocxxi. "
            "Pick a single file, a module, or the whole tree."
        )
    )
    group = parser.add_mutually_exclusive_group()
    group.add_argument(
        "--module",
        help=(
            "Module name like 'Base' or a path relative to the repo root "
            "(for example src/Blocxxi/Base)."
        ),
    )
    group.add_argument(
        "--file",
        help="Single source/header file, relative to the repo root or absolute.",
    )
    parser.add_argument(
        "--build-dir",
        help=(
            "Build directory containing compile_commands.json. "
            "Defaults to auto-detecting out/build-ninja."
        ),
    )
    parser.add_argument(
        "--config",
        default="Debug",
        help=(
            "Preferred build configuration when compile_commands.json contains "
            "multiple entries per file. Default: Debug"
        ),
    )
    parser.add_argument(
        "--clang-tidy",
        default="clang-tidy",
        help="clang-tidy executable to invoke. Default: clang-tidy",
    )
    parser.add_argument(
        "--config-file",
        default=".clang-tidy",
        help="clang-tidy config file. Default: .clang-tidy",
    )
    parser.add_argument(
        "--extra-arg",
        action="append",
        default=[],
        help="Extra argument forwarded via --extra-arg=VALUE. Repeatable.",
    )
    parser.add_argument(
        "--exclude",
        action="append",
        default=[],
        help=(
            "Glob pattern for repo-relative files to skip. Repeatable. "
            "Example: --exclude **/loguru.cpp"
        ),
    )
    parser.add_argument(
        "-j",
        "--jobs",
        type=int,
        default=1,
        help="Number of parallel clang-tidy processes. Default: 1",
    )
    parser.add_argument(
        "--quiet",
        action="store_true",
        help="Pass --quiet to clang-tidy.",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Print the files and commands that would run without invoking clang-tidy.",
    )
    parser.add_argument(
        "--no-sanitize-config",
        action="store_true",
        help=(
            "Use the config file as-is. By default the script strips the "
            "ExtraArgs block into a temp config to avoid common Windows "
            "clang-tidy driver issues."
        ),
    )
    return parser.parse_args()


def find_build_dir(root: Path, explicit: str | None) -> Path:
    if explicit:
        build_dir = resolve_path(root, explicit)
        require_file(build_dir / "compile_commands.json", "compile_commands.json")
        return build_dir

    for candidate in DEFAULT_BUILD_DIR_CANDIDATES:
        if (root / candidate / "compile_commands.json").is_file():
            return root / candidate

    for candidate in sorted((root / "out").glob("*/compile_commands.json")):
        return candidate.parent

    raise FileNotFoundError(
        "Could not find compile_commands.json. "
        "Build the repo first or pass --build-dir."
    )


def resolve_path(root: Path, value: str) -> Path:
    path = Path(value)
    if not path.is_absolute():
        path = root / path
    return path.resolve()


def require_file(path: Path, label: str) -> None:
    if not path.is_file():
        raise FileNotFoundError(f"{label} not found: {path}")


def sanitize_config(config_path: Path) -> Path:
    text = config_path.read_text(encoding="utf-8")
    lines = text.splitlines(keepends=True)
    sanitized: list[str] = []
    skip_extra_args = False

    for line in lines:
        if re.match(r"^\s*ExtraArgs:\s*$", line):
            skip_extra_args = True
            continue

        if skip_extra_args:
            if re.match(r"^\s*-\s+", line):
                continue
            skip_extra_args = False

        sanitized.append(line)

    temp_dir = Path(tempfile.mkdtemp(prefix="nova-tidy-"))
    sanitized_path = temp_dir / config_path.name
    sanitized_path.write_text("".join(sanitized), encoding="utf-8")
    return sanitized_path


def load_compile_commands(build_dir: Path) -> list[dict]:
    return json_load(build_dir / "compile_commands.json")


def json_load(path: Path) -> list[dict]:
    import json

    return json.loads(path.read_text(encoding="utf-8"))


def write_filtered_compile_commands(
    root: Path, build_dir: Path, config: str, entries: list[dict]
) -> Path:
    import json

    config_token_slash = f"/{config}/"
    config_token_backslash = f"\\{config}\\"
    filtered = [
        entry
        for entry in entries
        if config_token_slash in entry.get("output", "")
        or config_token_backslash in entry.get("output", "")
        or f'CMAKE_INTDIR=\\"{config}\\"' in entry.get("command", "")
    ]

    if not filtered:
        return build_dir

    sanitized = [sanitize_compile_command_entry(entry) for entry in filtered]

    temp_dir = Path(tempfile.mkdtemp(prefix="nova-tidy-cc-"))
    (temp_dir / "compile_commands.json").write_text(
        json.dumps(sanitized, indent=2), encoding="utf-8"
    )
    return temp_dir


def sanitize_compile_command_entry(entry: dict) -> dict:
    sanitized = dict(entry)
    arguments = extract_compile_arguments(entry)
    if arguments is None:
        return sanitized

    sanitized["arguments"] = [
        normalize_tidy_arg(arg)
        for arg in arguments
        if not should_strip_tidy_arg(arg)
    ]
    sanitized.pop("command", None)
    return sanitized


def extract_compile_arguments(entry: dict) -> list[str] | None:
    if "arguments" in entry and isinstance(entry["arguments"], list):
        return list(entry["arguments"])

    command = entry.get("command")
    if not command:
        return None

    return shlex.split(command, posix=False)


def should_strip_tidy_arg(arg: str) -> bool:
    return arg == "/MP" or arg == "/Zc:preprocessor"


def normalize_tidy_arg(arg: str) -> str:
    return arg.replace('\\"', '"')


def detect_standard_flag(entries: list[dict], config: str) -> str | None:
    preferred = [
        entry
        for entry in entries
        if f'\\{config}\\' in entry.get("output", "")
        or f"/{config}/" in entry.get("output", "")
    ]
    search_space = preferred or entries
    pattern = re.compile(r"(/std:[^\s]+|-std=[^\s]+)")
    for entry in search_space:
        match = pattern.search(entry.get("command", ""))
        if match:
            return match.group(1)
    return None


def default_extra_args(extra_args: list[str], detected_standard: str | None) -> list[str]:
    result = list(extra_args)
    default_warning_suppressions = [
        "-Wno-unused-command-line-argument",
        "-Wno-pragma-once-outside-header",
    ]

    for arg in default_warning_suppressions:
        if arg not in result:
            result.append(arg)

    if detected_standard and not any(
        arg.startswith("/std:") or arg.startswith("-std=") for arg in result
    ):
        result.append(detected_standard)
    elif platform.system() == "Windows" and not any(
        arg.startswith("/std:") or arg.startswith("-std=") for arg in result
    ):
        result.append("/std:c++latest")
    return result


def resolve_targets(root: Path, module: str | None, file_value: str | None) -> list[Path]:
    if file_value:
        file_path = resolve_path(root, file_value)
        require_file(file_path, "target file")
        return [file_path]

    if module:
        module_path = resolve_module_path(root, module)
        return collect_source_files(module_path)

    return collect_source_files(root / "src" / "Blocxxi")


def apply_excludes(root: Path, targets: list[Path], patterns: list[str]) -> list[Path]:
    if not patterns:
        return targets

    filtered = [
        target for target in targets if not matches_any_exclude(root, target, patterns)
    ]
    if not filtered:
        raise FileNotFoundError(
            "All candidate files were excluded. Adjust --exclude patterns or target selection."
        )
    return filtered


def matches_any_exclude(root: Path, target: Path, patterns: list[str]) -> bool:
    relative = PurePosixPath(target.relative_to(root).as_posix())
    return any(matches_exclude_pattern(relative, pattern) for pattern in patterns)


def matches_exclude_pattern(relative: PurePosixPath, pattern: str) -> bool:
    normalized_pattern = pattern.replace("\\", "/")
    if relative.match(normalized_pattern):
        return True
    if normalized_pattern.startswith("**/"):
        return relative.match(normalized_pattern[3:])
    return False


def resolve_module_path(root: Path, module: str) -> Path:
    raw_path = Path(module)
    if raw_path.is_absolute() or raw_path.parts:
        candidate = resolve_path(root, module)
        if candidate.exists():
            if candidate.is_file():
                return candidate.parent
            return candidate

    candidate = (root / "src" / "Blocxxi" / module).resolve()
    if candidate.exists():
        return candidate

    raise FileNotFoundError(
        f"Module '{module}' was not found under src/Blocxxi or as a repo-relative path."
    )


def collect_source_files(base: Path) -> list[Path]:
    if base.is_file():
        return [base]
    if not base.is_dir():
        raise FileNotFoundError(f"Target directory not found: {base}")

    files = sorted(
        path
        for path in base.rglob("*")
        if path.is_file() and path.suffix.lower() in SOURCE_SUFFIXES
    )
    if not files:
        raise FileNotFoundError(f"No supported source files found under: {base}")
    return files


def command_for_file(
    clang_tidy: str,
    build_dir: Path,
    config_file: Path,
    file_path: Path,
    extra_args: Iterable[str],
    quiet: bool,
) -> list[str]:
    command = [
        clang_tidy,
        "-p",
        str(build_dir),
        "--config-file",
        str(config_file),
    ]
    for arg in extra_args:
        command.append(f"--extra-arg={arg}")
    if quiet:
        command.append("--quiet")
    command.append(str(file_path))
    return command


def run_one(command: list[str], file_path: Path) -> TidyResult:
    completed = subprocess.run(
        command,
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
        check=False,
    )
    output = (completed.stdout + completed.stderr).strip()
    return TidyResult(file=file_path, returncode=completed.returncode, output=output)


def main() -> int:
    args = parse_args()
    root = repo_root()

    try:
        build_dir = find_build_dir(root, args.build_dir)
        compile_commands = load_compile_commands(build_dir)
        effective_build_dir = write_filtered_compile_commands(
            root, build_dir, args.config, compile_commands
        )
        config_path = resolve_path(root, args.config_file)
        require_file(config_path, "clang-tidy config")
        config_to_use = (
            config_path if args.no_sanitize_config else sanitize_config(config_path)
        )
        targets = resolve_targets(root, args.module, args.file)
        targets = apply_excludes(root, targets, args.exclude)
        detected_standard = detect_standard_flag(compile_commands, args.config)
        extra_args = default_extra_args(args.extra_arg, detected_standard)
    except FileNotFoundError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2

    commands = [
        command_for_file(
            clang_tidy=args.clang_tidy,
            build_dir=effective_build_dir,
            config_file=config_to_use,
            file_path=file_path,
            extra_args=extra_args,
            quiet=args.quiet,
        )
        for file_path in targets
    ]

    print(f"Repo root:   {root}")
    print(f"Build dir:   {build_dir}")
    if effective_build_dir != build_dir:
        print(f"Using ccdb:  {effective_build_dir}")
    print(f"Config file: {config_to_use}")
    if args.exclude:
        print(f"Excludes:    {', '.join(args.exclude)}")
    print(f"Files:       {len(targets)}")

    if args.dry_run:
        for command in commands:
            print(" ".join(subprocess.list2cmdline([part]) for part in command))
        return 0

    if args.jobs < 1:
        print("error: --jobs must be at least 1", file=sys.stderr)
        return 2

    failures = 0
    with ThreadPoolExecutor(max_workers=args.jobs) as executor:
        futures = {
            executor.submit(run_one, command, file_path): file_path
            for command, file_path in zip(commands, targets, strict=True)
        }
        for future in as_completed(futures):
            result = future.result()
            relative = result.file.relative_to(root)
            status = "ok" if result.returncode == 0 else "fail"
            print(f"[{status}] {relative}")
            if result.output:
                print(result.output)
                print()
            if result.returncode != 0:
                failures += 1

    print(
        f"Completed clang-tidy on {len(targets)} file(s) with {failures} failing invocation(s)."
    )
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
