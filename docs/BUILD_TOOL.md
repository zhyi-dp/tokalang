# Toka Build Tool (`toka`) Usage Guide

`toka` is the official build system and project manager for the Toka programming language. It is designed to handle project configuration, compilation, and execution using a "configuration as code" approach via `Project.tk`.

## Installation

Currently, the `toka` tool is built from source.

1. Ensure you have the Toka compiler (`tokac`) and `clang` (LLVM 17+) installed and in your `PATH`.
2. Compile the tool:
   ```bash
   tokac tools/toka/src/main.tk > build/toka.ll
   clang build/toka.ll -o build/toka -isysroot $(xcrun --show-sdk-path)
   ```
3. (Optional) Add `build/toka` to your `PATH`.

## Commands

### `toka new <project_name>`
Creates a new Toka project directory with a standard structure.

**Example:**
```bash
toka new my_project
```

**Created Structure:**
- `my_project/`
  - `Project.tk`: The build script and manifest.
  - `src/`
    - `main.tk`: The main entry point for your application logic.

### `toka build`
Compiles the current project based on the instructions in `Project.tk`.

- It executes `Project.tk` natively.
- It generates LLVM IR for your source files.
- It uses `clang` to link the final executable.
- Output is located in `target/debug/`.

### `toka run`
Compiles and immediately executes the project.

```bash
toka run
```

## Configuration: `Project.tk`

Toka uses its own language for build configuration. This allows for powerful, programmable build logic.

**Standard `Project.tk` template:**
```toka
import build::{Executable, run_build}

fn main() {
    auto app = Executable(
        name = "app",
        version = "0.1.0",
        src = "src/main.tk"
    )
    run_build(app)
}
```

## Environment Variables

The `toka` tool and `tokac` compiler respect the following environment variables if you need to override default behaviors:

- `TOKA_LIB_PATH`: **CRITICAL**. Path to the Toka standard library (the `lib/` directory). Required for compiling projects from subdirectories. You can provide multiple paths separated by `:`.
- `TOKA_CLANG`: Path to the `clang` compiler to use for linking. **Ensure this matches the LLVM version of your `tokac`** (currently LLVM 20).
- `PATH`: Ensure `/Users/zhyi/GitDP/toka/build/src` and `/Users/zhyi/GitDP/toka/build` are in your `PATH` so `toka` and `tokac` can be found globally.

## Recommended Shell Configuration (.zshrc)

For the best experience, add the following to your `~/.zshrc`:

```bash
export PATH="$PATH:/Users/zhyi/GitDP/toka/build/src:/Users/zhyi/GitDP/toka/build"
export TOKA_LIB_PATH="/Users/zhyi/GitDP/toka/lib"
export TOKA_CLANG="/usr/local/opt/llvm@20/bin/clang"
```

## Troubleshooting

- **"Module 'build' not found"**: Ensure `TOKA_LIB_PATH` is set correctly to the absolute path of the `lib/` directory.
- **LLVM IR Syntax Error (e.g. `nuw` or `expected type`)**: Your `clang` version is too old for the LLVM IR emitted by `tokac`. Set `TOKA_CLANG` to a version that matches the `tokac` LLVM backend (LLVM 17-20).
- **"tokac: command not found"**: Ensure the directory containing the `tokac` binary is in your `PATH`.
