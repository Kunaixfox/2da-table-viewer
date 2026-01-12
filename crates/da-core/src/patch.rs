//! Patch and export functionality for editing 2DA tables
//!
//! This module provides:
//! - Edit tracking for cells in a merged table
//! - Patch file format (JSON) for storing edits
//! - Export functionality that writes edits back to source files

use crate::error::{Error, Result};
use crate::merger::ResolvedTable;
use crate::parser::parse_csv;
use serde::{Deserialize, Serialize};
use std::collections::HashMap;
use std::fs::{self, File};
use std::io::{BufWriter, Write};
use std::path::{Path, PathBuf};

/// A single edit to a cell
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Edit {
    /// Row ID (must match a row in the table)
    pub row_id: i64,
    /// Column name
    pub column: String,
    /// New value as a string
    pub value: String,
}

impl Edit {
    /// Create a new edit
    pub fn new(row_id: i64, column: impl Into<String>, value: impl Into<String>) -> Self {
        Self {
            row_id,
            column: column.into(),
            value: value.into(),
        }
    }
}

/// A patch file containing multiple edits for a family
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PatchFile {
    /// Family name this patch applies to
    pub family: String,
    /// List of edits
    pub edits: Vec<Edit>,
}

impl PatchFile {
    /// Create a new empty patch file
    pub fn new(family: impl Into<String>) -> Self {
        Self {
            family: family.into(),
            edits: Vec::new(),
        }
    }

    /// Add an edit to the patch
    pub fn add_edit(&mut self, edit: Edit) {
        self.edits.push(edit);
    }

    /// Load a patch file from JSON
    pub fn load<P: AsRef<Path>>(path: P) -> Result<Self> {
        let content = fs::read_to_string(path.as_ref()).map_err(|e| Error::FileRead {
            path: path.as_ref().to_path_buf(),
            source: e,
        })?;
        serde_json::from_str(&content).map_err(Error::Json)
    }

    /// Save the patch file to JSON
    pub fn save<P: AsRef<Path>>(&self, path: P) -> Result<()> {
        let content = serde_json::to_string_pretty(self)?;
        fs::write(path, content)?;
        Ok(())
    }
}

/// A batch file containing multiple patch operations
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct BatchFile {
    /// Root directories to scan
    pub roots: Vec<PathBuf>,
    /// Output directory for exports
    pub output_dir: PathBuf,
    /// List of patch files to apply
    pub patches: Vec<PathBuf>,
}

impl BatchFile {
    /// Load a batch file from JSON
    pub fn load<P: AsRef<Path>>(path: P) -> Result<Self> {
        let content = fs::read_to_string(path.as_ref()).map_err(|e| Error::FileRead {
            path: path.as_ref().to_path_buf(),
            source: e,
        })?;
        serde_json::from_str(&content).map_err(Error::Json)
    }

    /// Save the batch file to JSON
    pub fn save<P: AsRef<Path>>(&self, path: P) -> Result<()> {
        let content = serde_json::to_string_pretty(self)?;
        fs::write(path, content)?;
        Ok(())
    }
}

/// Result of applying a patch - tracks which source files were modified
#[derive(Debug, Clone)]
pub struct PatchResult {
    /// Family name
    pub family: String,
    /// Number of edits applied
    pub edits_applied: usize,
    /// Source files that were modified (path -> list of row IDs changed)
    pub modified_sources: HashMap<PathBuf, Vec<i64>>,
    /// Edits that failed (row not found, column not found, etc.)
    pub failed_edits: Vec<(Edit, String)>,
}

/// Apply a patch to a resolved table and track which source files are affected
pub fn apply_patch(table: &ResolvedTable, patch: &PatchFile) -> Result<PatchResult> {
    let mut result = PatchResult {
        family: patch.family.clone(),
        edits_applied: 0,
        modified_sources: HashMap::new(),
        failed_edits: Vec::new(),
    };

    for edit in &patch.edits {
        // Find the row by ID
        let row_idx = table.rows.iter().position(|r| r.id == Some(edit.row_id));

        let row_idx = match row_idx {
            Some(idx) => idx,
            None => {
                result.failed_edits.push((
                    edit.clone(),
                    format!("Row ID {} not found", edit.row_id),
                ));
                continue;
            }
        };

        // Find the column by name
        let col_idx = table.columns.iter().position(|c| c.name == edit.column);

        let col_idx = match col_idx {
            Some(idx) => idx,
            None => {
                result.failed_edits.push((
                    edit.clone(),
                    format!("Column '{}' not found", edit.column),
                ));
                continue;
            }
        };

        // Get the source file for this cell
        let source = &table.rows[row_idx].cells[col_idx].source;

        // Track this modification
        result
            .modified_sources
            .entry(source.clone())
            .or_default()
            .push(edit.row_id);

        result.edits_applied += 1;
    }

    Ok(result)
}

/// Export modified source files with edits applied
///
/// This reads the original source files, applies the relevant edits,
/// and writes new copies to the output directory.
pub fn export_with_edits<P: AsRef<Path>>(
    table: &ResolvedTable,
    patch: &PatchFile,
    output_dir: P,
) -> Result<ExportResult> {
    let output_dir = output_dir.as_ref();

    // Create output directory if it doesn't exist
    fs::create_dir_all(output_dir)?;

    // Group edits by source file
    let mut edits_by_source: HashMap<PathBuf, Vec<&Edit>> = HashMap::new();

    for edit in &patch.edits {
        // Find the row and get its source file for the edited column
        if let Some(row) = table.rows.iter().find(|r| r.id == Some(edit.row_id)) {
            if let Some(col) = table.columns.iter().find(|c| c.name == edit.column) {
                let source = &row.cells[col.index].source;
                edits_by_source
                    .entry(source.clone())
                    .or_default()
                    .push(edit);
            }
        }
    }

    let mut result = ExportResult {
        files_written: Vec::new(),
        edits_applied: 0,
        errors: Vec::new(),
    };

    // Process each source file that has edits
    for (source_path, edits) in edits_by_source {
        match export_single_file(&source_path, &edits, output_dir) {
            Ok(output_path) => {
                result.edits_applied += edits.len();
                result.files_written.push(output_path);
            }
            Err(e) => {
                result.errors.push((source_path, e.to_string()));
            }
        }
    }

    Ok(result)
}

/// Export a single source file with edits applied
fn export_single_file(
    source_path: &Path,
    edits: &[&Edit],
    output_dir: &Path,
) -> Result<PathBuf> {
    // Parse the original file
    let original = parse_csv(source_path)?;

    // Build a map of edits: (row_id, column_name) -> new_value
    let edit_map: HashMap<(i64, &str), &str> = edits
        .iter()
        .map(|e| ((e.row_id, e.column.as_str()), e.value.as_str()))
        .collect();

    // Build column name -> index map
    let col_indices: HashMap<&str, usize> = original
        .columns
        .iter()
        .map(|c| (c.name.as_str(), c.index))
        .collect();

    // Determine output path
    let file_name = source_path
        .file_name()
        .ok_or_else(|| Error::InvalidFamilyName("Invalid source path".to_string()))?;
    let output_path = output_dir.join(file_name);

    // Write the modified CSV
    let file = File::create(&output_path)?;
    let mut writer = BufWriter::new(file);

    // Write header
    let header: Vec<&str> = original.columns.iter().map(|c| c.name.as_str()).collect();
    writeln!(writer, "{}", header.join(","))?;

    // Write rows with edits applied
    for row in &original.rows {
        let mut cells: Vec<String> = row
            .cells
            .iter()
            .map(|c| c.to_string_value())
            .collect();

        // Apply any edits for this row
        if let Some(row_id) = row.id {
            for (col_name, &col_idx) in &col_indices {
                if let Some(&new_value) = edit_map.get(&(row_id, col_name)) {
                    if col_idx < cells.len() {
                        cells[col_idx] = new_value.to_string();
                    }
                }
            }
        }

        // Escape and write
        let escaped: Vec<String> = cells.iter().map(|c| escape_csv(c)).collect();
        writeln!(writer, "{}", escaped.join(","))?;
    }

    Ok(output_path)
}

/// Result of exporting with edits
#[derive(Debug, Clone)]
pub struct ExportResult {
    /// Files that were written
    pub files_written: Vec<PathBuf>,
    /// Total number of edits applied
    pub edits_applied: usize,
    /// Errors encountered (source path, error message)
    pub errors: Vec<(PathBuf, String)>,
}

/// Escape a value for CSV output
fn escape_csv(s: &str) -> String {
    if s.contains(',') || s.contains('"') || s.contains('\n') || s.contains('\r') {
        format!("\"{}\"", s.replace('"', "\"\""))
    } else {
        s.to_string()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_edit_creation() {
        let edit = Edit::new(42, "Name", "NewValue");
        assert_eq!(edit.row_id, 42);
        assert_eq!(edit.column, "Name");
        assert_eq!(edit.value, "NewValue");
    }

    #[test]
    fn test_patch_file_serialization() {
        let mut patch = PatchFile::new("test_family");
        patch.add_edit(Edit::new(1, "Col1", "Value1"));
        patch.add_edit(Edit::new(2, "Col2", "Value2"));

        let json = serde_json::to_string_pretty(&patch).unwrap();
        let loaded: PatchFile = serde_json::from_str(&json).unwrap();

        assert_eq!(loaded.family, "test_family");
        assert_eq!(loaded.edits.len(), 2);
        assert_eq!(loaded.edits[0].row_id, 1);
        assert_eq!(loaded.edits[1].column, "Col2");
    }

    #[test]
    fn test_escape_csv() {
        assert_eq!(escape_csv("simple"), "simple");
        assert_eq!(escape_csv("with,comma"), "\"with,comma\"");
        assert_eq!(escape_csv("with\"quote"), "\"with\"\"quote\"");
        assert_eq!(escape_csv("with\nnewline"), "\"with\nnewline\"");
    }
}
