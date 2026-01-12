//! da-core: Core library for parsing and merging Dragon Age 2DA tables
//!
//! This library provides functionality to:
//! - Scan directories for CSV files (exported from 2DA format)
//! - Parse CSV files into structured tables
//! - Group files into "families" based on naming conventions
//! - Merge family members with provenance tracking
//! - Apply patches (edits) and export modified source files

pub mod error;
pub mod merger;
pub mod parser;
pub mod patch;
pub mod scanner;
pub mod table;

pub use error::{Error, Result};
pub use merger::{merge_family, ResolvedCell, ResolvedRow, ResolvedTable};
pub use parser::parse_csv;
pub use patch::{apply_patch, export_with_edits, BatchFile, Edit, ExportResult, PatchFile, PatchResult};
pub use scanner::{scan_directory, Family, FamilyMember};
pub use table::{CellValue, Column, Row, Table};
