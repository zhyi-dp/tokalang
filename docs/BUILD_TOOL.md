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

The `toka` tool respects the following environment variables if you need to override default behaviors:

- `TOKA_CLANG`: Path to the `clang` compiler to use for linking (defaults to `/usr/local/opt/llvm@20/bin/clang` or system `clang`).
- `TOKA_LIB_PATH`: Path to the Toka standard library (if not in default locations).

## Troubleshooting

- **"Module 'build' not found"**: Ensure your `lib/` directory is accessible. `tokac` looks for the standard library in `./lib` and `../lib`.
- **Linking errors**: Ensure `clang` is installed and the macOS SDK path is correct (checked via `xcrun --show-sdk-path`).
