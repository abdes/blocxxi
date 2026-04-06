# Nova (loguru) Logging

This document describes the Nova logging system, how to configure it, and
how to use command line arguments and vmodule filtering effectively.

## Overview

- The system is based on loguru.
- A **global cutoff** controls the maximum verbosity that can be emitted.
- vmodule rules **only further restrict** logging and never override the
 global cutoff.
- Console logging can be disabled when file logging is enabled.

## Configuration Basics

Initialize logging early in `main()`:

```cpp
int main(int argc, const char** argv)
{
 loguru::Options options;
 loguru::init(argc, argv, options);
 // ...
}
```

Common runtime controls:

- `loguru::g_global_verbosity`: global cutoff (max verbosity allowed)
- `loguru::g_log_to_stderr`: enable/disable console output

Example:

```cpp
loguru::g_global_verbosity = loguru::Verbosity_WARNING;
loguru::g_log_to_stderr = true;
```

## Command Line Arguments

Loguru processes and removes its own flags from `argv`.

### Global cutoff

`-v` sets the global cutoff (required when you want vmodule filtering to be
meaningful):

```text
-v 2
-v=WARNING
-v OFF
```

Rule: a message logs only if its verbosity is **<= global cutoff**.

### Log file

`-L` / `--logfile` adds a file sink and disables console output.

- `-L path` truncates the file
- `-L +path` appends

Examples:

```text
-L logs.log
-L +logs.log
--logfile=logs.log
--logfile=+logs.log
```

### vmodule

`--vmodule` provides module-specific filtering rules. Rules are applied in
order; **first match wins**.

Pattern behavior:

- `*` matches within a path segment (does not cross `/`)
- `**` matches across path segments
- `?` matches a single character
- If the pattern contains `/` or `\`, it matches the **full path**
- Otherwise, it matches the **basename** (file name without extension), with
 `-inl` suffix trimmed

Important: vmodule rules **cannot exceed** the global cutoff.

## Files vs Console Logging

- Console output is enabled by default.
- If you pass `-L`/`--logfile`, console output is disabled.
- File output always respects the global cutoff.

## vmodule Examples

### 1) Only one module at verbosity 2

```text
-v=2 --vmodule="**/AssetLoader=2,*=OFF"
```

- `AssetLoader` is allowed up to 2.
- `*=OFF` disables everything else.
- Order matters: specific rule must come first.

### 2) Silence a noisy module

```text
-v=2 --vmodule="**/MainModule=OFF"
```

### 3) Full path match

```text
-v=2 --vmodule="src/content/*=2,*=OFF"
```

This applies to files under `src/content/` only.

### 4) Basename match (no extension)

```text
-v=2 --vmodule="TextureBinder=2,*=OFF"
```

Matches `TextureBinder.cpp`, `TextureBinder.h`, etc.

### 5) Recursive directory match

```text
-v=3 --vmodule="**/render/**=3,*=OFF"
```

### 6) Combining multiple rules

```text
-v=3 --vmodule="**/AssetLoader=2,**/TextureBinder=3,*=OFF"
```

### 7) Disable everything (global)

```text
-v=OFF
```

## Common Pitfalls

- **Forgetting `-v`**: without a global cutoff, vmodule rules may not behave
 as expected.
- **Wrong rule order**: `*=OFF` must be last or it will mask everything.
- **Using file extensions in patterns**: patterns match without extensions.

## Recommended Recipes

- **Only one module**:
 `-v=2 --vmodule="**/AssetLoader=2,*=OFF"`
- **Everything off except warnings**:
 `-v=WARNING`
- **File logging only**:
 `-v=2 -L logs.log`
