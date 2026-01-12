//! Directory scanner for discovering and grouping 2DA CSV files

use crate::error::Result;
use serde::{Deserialize, Serialize};
use std::collections::BTreeMap;
use std::path::{Path, PathBuf};
use walkdir::WalkDir;

/// A family of related CSV files that should be merged together
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Family {
    /// Base name of the family (e.g., "abi_base")
    pub name: String,
    /// Members of this family, sorted by precedence
    pub members: Vec<FamilyMember>,
}

impl Family {
    /// Get the base file (no suffix) if it exists
    pub fn base_file(&self) -> Option<&FamilyMember> {
        self.members.iter().find(|m| m.suffix.is_none())
    }

    /// Get variant files (with suffix), sorted alphabetically
    pub fn variants(&self) -> Vec<&FamilyMember> {
        self.members
            .iter()
            .filter(|m| m.suffix.is_some())
            .collect()
    }
}

/// A member of a family (single CSV file)
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct FamilyMember {
    /// Full path to the file
    pub path: PathBuf,
    /// Suffix (e.g., "kcc" for "abi_base_kcc.csv"), None for base file
    pub suffix: Option<String>,
}

/// Result of scanning directories
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ScanResult {
    /// Root directories that were scanned
    pub roots: Vec<PathBuf>,
    /// Discovered families, sorted by name
    pub families: Vec<Family>,
    /// Total number of files found
    pub total_files: usize,
}

impl ScanResult {
    /// Find a family by name
    pub fn find_family(&self, name: &str) -> Option<&Family> {
        self.families.iter().find(|f| f.name == name)
    }

    /// Get all family names
    pub fn family_names(&self) -> Vec<&str> {
        self.families.iter().map(|f| f.name.as_str()).collect()
    }
}

/// Scan one or more directories for CSV files and group them into families
pub fn scan_directory<P: AsRef<Path>>(roots: &[P]) -> Result<ScanResult> {
    let mut file_map: BTreeMap<String, Vec<(PathBuf, Option<String>)>> = BTreeMap::new();
    let mut total_files = 0;

    for root in roots {
        let root = root.as_ref();

        for entry in WalkDir::new(root)
            .follow_links(true)
            .into_iter()
            .filter_map(|e| e.ok())
        {
            let path = entry.path();

            // Only process CSV files
            if path.extension().is_some_and(|ext| ext == "csv") {
                if let Some(file_name) = path.file_stem().and_then(|s| s.to_str()) {
                    let (family_name, suffix) = extract_family_info(file_name);

                    file_map
                        .entry(family_name)
                        .or_default()
                        .push((path.to_path_buf(), suffix));

                    total_files += 1;
                }
            }
        }
    }

    // Convert to families
    let families: Vec<Family> = file_map
        .into_iter()
        .map(|(name, mut members)| {
            // Sort members: base file first, then variants alphabetically
            members.sort_by(|a, b| match (&a.1, &b.1) {
                (None, None) => a.0.cmp(&b.0),
                (None, Some(_)) => std::cmp::Ordering::Less,
                (Some(_), None) => std::cmp::Ordering::Greater,
                (Some(sa), Some(sb)) => sa.cmp(sb),
            });

            let members = members
                .into_iter()
                .map(|(path, suffix)| FamilyMember { path, suffix })
                .collect();

            Family { name, members }
        })
        .collect();

    Ok(ScanResult {
        roots: roots.iter().map(|r| r.as_ref().to_path_buf()).collect(),
        families,
        total_files,
    })
}

/// Extract family name and optional suffix from a filename
///
/// Examples:
/// - "abi_base" -> ("abi_base", None)
/// - "abi_base_kcc" -> ("abi_base", Some("kcc"))
/// - "achievements_ep1" -> ("achievements", Some("ep1"))
/// - "ai_abilities_cond_str" -> ("ai_abilities_cond", Some("str"))
fn extract_family_info(file_name: &str) -> (String, Option<String>) {
    // Known DLC/variant suffixes - these indicate a variant file
    const KNOWN_SUFFIXES: &[&str] = &[
        "drk", "ep1", "gib", "kcc", "lel", "mem", "shale", "str", "val", "vala", "toe", "hrm",
        "ibmoobs", "gxa",
    ];

    // Try to find a known suffix at the end
    for suffix in KNOWN_SUFFIXES {
        let suffix_pattern = format!("_{}", suffix);
        if file_name.ends_with(&suffix_pattern) {
            let base = &file_name[..file_name.len() - suffix_pattern.len()];
            return (base.to_string(), Some(suffix.to_string()));
        }
    }

    // No known suffix found - treat as base file
    (file_name.to_string(), None)
}

/// Check if a filename looks like a variant (has underscore + short suffix)
/// This is a heuristic for files not in the known suffix list
#[allow(dead_code)]
fn looks_like_variant(file_name: &str) -> bool {
    // Look for pattern: base_name_XYZ where XYZ is 2-5 characters
    if let Some(last_underscore) = file_name.rfind('_') {
        let suffix = &file_name[last_underscore + 1..];
        // Short suffix (2-5 chars) that's alphanumeric (e.g., "kcc", "ep1")
        suffix.len() >= 2
            && suffix.len() <= 5
            && suffix.chars().all(|c| c.is_ascii_alphanumeric())
    } else {
        false
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_extract_family_base() {
        let (family, suffix) = extract_family_info("abi_base");
        assert_eq!(family, "abi_base");
        assert_eq!(suffix, None);
    }

    #[test]
    fn test_extract_family_with_suffix() {
        let (family, suffix) = extract_family_info("abi_base_kcc");
        assert_eq!(family, "abi_base");
        assert_eq!(suffix, Some("kcc".to_string()));
    }

    #[test]
    fn test_extract_family_achievements() {
        let (family, suffix) = extract_family_info("achievements_ep1");
        assert_eq!(family, "achievements");
        assert_eq!(suffix, Some("ep1".to_string()));
    }

    #[test]
    fn test_extract_family_unknown_suffix() {
        // Unknown suffix should be treated as part of the base name
        let (family, suffix) = extract_family_info("some_table_xyz");
        assert_eq!(family, "some_table_xyz");
        assert_eq!(suffix, None);
    }

    #[test]
    fn test_looks_like_variant() {
        assert!(looks_like_variant("base_kcc"));
        assert!(looks_like_variant("something_ep1"));
        assert!(!looks_like_variant("no_suffix_here_toolong"));
        assert!(!looks_like_variant("single"));
    }
}
