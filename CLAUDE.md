# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

DA Table Viewer - a Rust tool for scanning, parsing, and merging Dragon Age 2DA table CSV files with provenance tracking. Designed with a Qt 6 Widgets UI frontend connected via C FFI.

## Build Commands

```bash
# Build all crates
cargo build --workspace

# Build release
cargo build --workspace --release

# Run tests
cargo test --workspace

# Run clippy lints
cargo clippy --workspace

# Format code
cargo fmt --all
```

## CLI Usage

```bash
# Scan directories for CSV files
da-cli scan --root <path>

# List discovered families
da-cli list-families --root <path> [--verbose]

# Show merged table
da-cli show --root <path> --family <name> [--limit N] [--columns col1,col2]

# Export merged table
da-cli export --root <path> --family <name> --format csv|json --output <file>

# Explain cell provenance
da-cli explain --root <path> --family <name> --row <id> --col <name>

# Parse single file
da-cli parse --file <path>

# Create a patch file template
da-cli create-patch --family <name> --output patch.json [--example "row:col:value"]

# Apply a patch and export modified source files
da-cli patch --root <path> --patch patch.json --output <dir>

# Create a batch file template
da-cli create-batch --root <path> --export-dir <dir> --output batch.json

# Run a batch of patches
da-cli batch --batch batch.json
```

## Patch/Export Workflow

1. View merged table, identify changes needed
2. Create patch file: `da-cli create-patch --family achievements --output my_patch.json`
3. Edit JSON to add your changes:
   ```json
   {
     "family": "achievements",
     "edits": [
       {"row_id": 0, "column": "Name", "value": "NewName"},
       {"row_id": 5, "column": "Points", "value": "999"}
     ]
   }
   ```
4. Apply and export: `da-cli patch --root ./2da --patch my_patch.json --output exports/`
5. Result: Modified source files (copies) appear in exports/, originals untouched

## Repository Structure

```
crates/
  da-core/        # Pure Rust library - parsing, merging, provenance
    src/
      lib.rs      # Public API exports
      error.rs    # Error types (thiserror)
      table.rs    # Table, Row, Column, CellValue types
      parser.rs   # CSV parsing
      scanner.rs  # File discovery, family grouping
      merger.rs   # Merge engine with provenance
      patch.rs    # Patch/edit application and export
  da-cli/         # CLI binary using clap
  da-ffi/         # C FFI layer (cdylib) for Qt integration
```

## Key Concepts

- **Family**: Files sharing a base name that merge into one logical table
  - Base file: `achievements.csv`
  - Variants: `achievements_ep1.csv`, `achievements_drk.csv`
- **Resolved table**: Merged result with column union and row overrides
- **Provenance**: Each cell tracks which source file provided its value

## Architecture Constraints

- `da-core` has zero Qt/FFI dependencies - all business logic lives here
- FFI layer is thin: exposes C functions, handles memory lifecycle
- Qt UI (future) calls FFI only; no business logic in C++
- All `unsafe` code isolated to `da-ffi`

## Merge Rules

1. Base file (no suffix) applies first
2. Variants apply in alphabetical order by suffix
3. Non-empty cells override previous values
4. Empty cells do not override (preserve base value)
5. New rows are added; existing rows merge by ID

## Known Suffixes

DLC/variant suffixes recognized: `drk`, `ep1`, `gib`, `kcc`, `lel`, `mem`, `shale`, `str`, `val`, `vala`, `toe`, `hrm`, `ibmoobs`, `gxa`

## Testing

```bash
# Run all tests
cargo test --workspace

# Run specific test
cargo test -p da-core test_merge_override_cells
```

Unit tests cover:
- Cell value parsing (integer, float, string, empty)
- Family name extraction from filenames
- CSV parsing with various edge cases
- Merge behavior (overrides, new rows, column union)
- Provenance tracking
