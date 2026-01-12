//! Error types for da-core

use std::path::PathBuf;
use thiserror::Error;

/// Result type alias using our Error type
pub type Result<T> = std::result::Result<T, Error>;

/// Errors that can occur in da-core
#[derive(Debug, Error)]
pub enum Error {
    /// Failed to read a file
    #[error("failed to read file '{path}': {source}")]
    FileRead {
        path: PathBuf,
        #[source]
        source: std::io::Error,
    },

    /// Failed to parse CSV
    #[error("failed to parse CSV '{path}': {message}")]
    CsvParse { path: PathBuf, message: String },

    /// CSV parsing error from the csv crate
    #[error("CSV error in '{path}': {source}")]
    Csv {
        path: PathBuf,
        #[source]
        source: csv::Error,
    },

    /// Directory traversal error
    #[error("failed to traverse directory: {0}")]
    WalkDir(#[from] walkdir::Error),

    /// No files found for a family
    #[error("no files found for family '{0}'")]
    FamilyNotFound(String),

    /// Invalid family name
    #[error("invalid family name: {0}")]
    InvalidFamilyName(String),

    /// Column mismatch during merge
    #[error("column mismatch: expected '{expected}', found '{found}' in {path}")]
    ColumnMismatch {
        expected: String,
        found: String,
        path: PathBuf,
    },

    /// Row ID conflict
    #[error("duplicate row ID {id} in {path}")]
    DuplicateRowId { id: i64, path: PathBuf },

    /// IO error
    #[error("IO error: {0}")]
    Io(#[from] std::io::Error),

    /// JSON serialization error
    #[error("JSON error: {0}")]
    Json(#[from] serde_json::Error),
}
