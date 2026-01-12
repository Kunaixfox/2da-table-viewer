//! History tracking for applied patches
//!
//! Tracks which patches have been applied to allow undo operations.

use crate::error::{Error, Result};
use crate::patch::PatchFile;
use chrono::{DateTime, Utc};
use serde::{Deserialize, Serialize};
use std::collections::HashMap;
use std::fs;
use std::path::{Path, PathBuf};

/// A record of a patch that was applied
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct HistoryEntry {
    /// When the patch was applied
    pub timestamp: DateTime<Utc>,
    /// Family that was patched
    pub family: String,
    /// The patch that was applied
    pub patch: PatchFile,
    /// Files that were created/modified
    pub output_files: Vec<PathBuf>,
    /// Output directory used
    pub output_dir: PathBuf,
}

/// History file containing all applied patches
#[derive(Debug, Clone, Serialize, Deserialize, Default)]
pub struct HistoryFile {
    /// History entries grouped by family name
    pub entries: HashMap<String, Vec<HistoryEntry>>,
}

impl HistoryFile {
    /// Create a new empty history
    pub fn new() -> Self {
        Self::default()
    }

    /// Load history from a file, or create empty if not exists
    pub fn load<P: AsRef<Path>>(path: P) -> Result<Self> {
        let path = path.as_ref();
        if !path.exists() {
            return Ok(Self::new());
        }

        let content = fs::read_to_string(path).map_err(|e| Error::FileRead {
            path: path.to_path_buf(),
            source: e,
        })?;
        serde_json::from_str(&content).map_err(Error::Json)
    }

    /// Save history to a file
    pub fn save<P: AsRef<Path>>(&self, path: P) -> Result<()> {
        let content = serde_json::to_string_pretty(self)?;
        fs::write(path, content)?;
        Ok(())
    }

    /// Add an entry to the history
    pub fn add_entry(&mut self, entry: HistoryEntry) {
        self.entries
            .entry(entry.family.clone())
            .or_default()
            .push(entry);
    }

    /// Get history for a specific family
    pub fn get_family_history(&self, family: &str) -> Option<&Vec<HistoryEntry>> {
        self.entries.get(family)
    }

    /// Get the last entry for a family (for undo)
    pub fn get_last_entry(&self, family: &str) -> Option<&HistoryEntry> {
        self.entries.get(family).and_then(|v| v.last())
    }

    /// Remove and return the last entry for a family (for undo)
    pub fn pop_last_entry(&mut self, family: &str) -> Option<HistoryEntry> {
        self.entries.get_mut(family).and_then(|v| v.pop())
    }

    /// Get all families that have history
    pub fn families(&self) -> Vec<&str> {
        self.entries.keys().map(|s| s.as_str()).collect()
    }

    /// Get total number of entries
    pub fn total_entries(&self) -> usize {
        self.entries.values().map(|v| v.len()).sum()
    }
}

/// Create a history entry from a successful patch application
pub fn create_history_entry(
    patch: &PatchFile,
    output_files: Vec<PathBuf>,
    output_dir: PathBuf,
) -> HistoryEntry {
    HistoryEntry {
        timestamp: Utc::now(),
        family: patch.family.clone(),
        patch: patch.clone(),
        output_files,
        output_dir,
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::patch::Edit;

    #[test]
    fn test_history_add_and_get() {
        let mut history = HistoryFile::new();

        let mut patch = PatchFile::new("test_family");
        patch.add_edit(Edit::new(1, "col", "val"));

        let entry = create_history_entry(
            &patch,
            vec![PathBuf::from("output.csv")],
            PathBuf::from("exports"),
        );

        history.add_entry(entry);

        assert_eq!(history.total_entries(), 1);
        assert!(history.get_family_history("test_family").is_some());
        assert!(history.get_last_entry("test_family").is_some());
    }

    #[test]
    fn test_history_pop() {
        let mut history = HistoryFile::new();

        let mut patch = PatchFile::new("test_family");
        patch.add_edit(Edit::new(1, "col", "val"));

        let entry = create_history_entry(
            &patch,
            vec![PathBuf::from("output.csv")],
            PathBuf::from("exports"),
        );

        history.add_entry(entry);

        let popped = history.pop_last_entry("test_family");
        assert!(popped.is_some());
        assert_eq!(history.total_entries(), 0);
    }
}
