//! Merge engine for combining family tables with provenance tracking

use crate::error::{Error, Result};
use crate::parser::parse_csv;
use crate::scanner::Family;
use crate::table::{CellValue, Column, Table};
use serde::{Deserialize, Serialize};
use std::collections::{BTreeMap, HashSet};
use std::path::PathBuf;

/// A merged table with provenance information for each cell
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ResolvedTable {
    /// Family name
    pub family_name: String,
    /// Column definitions (union of all source columns)
    pub columns: Vec<Column>,
    /// Rows with provenance
    pub rows: Vec<ResolvedRow>,
    /// Files that contributed to this table, in merge order
    pub sources: Vec<PathBuf>,
}

impl ResolvedTable {
    /// Get the number of columns
    pub fn column_count(&self) -> usize {
        self.columns.len()
    }

    /// Get the number of rows
    pub fn row_count(&self) -> usize {
        self.rows.len()
    }

    /// Find a column by name
    pub fn find_column(&self, name: &str) -> Option<&Column> {
        self.columns.iter().find(|c| c.name == name)
    }

    /// Find a row by ID
    pub fn find_row(&self, id: i64) -> Option<&ResolvedRow> {
        self.rows.iter().find(|r| r.id == Some(id))
    }

    /// Get provenance for a specific cell
    pub fn get_provenance(&self, row_idx: usize, col_idx: usize) -> Option<&PathBuf> {
        self.rows
            .get(row_idx)
            .and_then(|r| r.cells.get(col_idx))
            .map(|c| &c.source)
    }
}

/// A row in the resolved table
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ResolvedRow {
    /// Row ID (from first column if numeric)
    pub id: Option<i64>,
    /// Cells with provenance
    pub cells: Vec<ResolvedCell>,
}

/// A cell with its value and source file
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ResolvedCell {
    /// The cell value
    pub value: CellValue,
    /// The file that provided this value
    pub source: PathBuf,
}

impl ResolvedCell {
    fn new(value: CellValue, source: PathBuf) -> Self {
        Self { value, source }
    }
}

/// Merge a family of tables into a single resolved table
pub fn merge_family(family: &Family) -> Result<ResolvedTable> {
    if family.members.is_empty() {
        return Err(Error::FamilyNotFound(family.name.clone()));
    }

    // Parse all member files
    let mut tables: Vec<Table> = Vec::new();
    for member in &family.members {
        let table = parse_csv(&member.path)?;
        tables.push(table);
    }

    merge_tables(&family.name, tables)
}

/// Merge multiple tables into a resolved table
pub fn merge_tables(family_name: &str, tables: Vec<Table>) -> Result<ResolvedTable> {
    if tables.is_empty() {
        return Err(Error::FamilyNotFound(family_name.to_string()));
    }

    // Build unified column list (union of all columns)
    let mut column_names: Vec<String> = Vec::new();
    let mut seen_columns: HashSet<String> = HashSet::new();

    for table in &tables {
        for col in &table.columns {
            if !seen_columns.contains(&col.name) {
                seen_columns.insert(col.name.clone());
                column_names.push(col.name.clone());
            }
        }
    }

    let columns: Vec<Column> = column_names
        .iter()
        .enumerate()
        .map(|(i, name)| Column::new(name.clone(), i))
        .collect();

    // Build column name -> index mapping for the unified columns
    let col_index: BTreeMap<&str, usize> = columns
        .iter()
        .map(|c| (c.name.as_str(), c.index))
        .collect();

    // Merge rows by ID
    // Using BTreeMap for deterministic ordering
    let mut rows_by_id: BTreeMap<i64, Vec<ResolvedCell>> = BTreeMap::new();
    let mut rows_without_id: Vec<(Vec<ResolvedCell>, PathBuf)> = Vec::new();

    let sources: Vec<PathBuf> = tables.iter().map(|t| t.source_path.clone()).collect();

    for table in &tables {
        // Build column mapping for this table
        let table_col_map: BTreeMap<&str, usize> = table
            .columns
            .iter()
            .map(|c| (c.name.as_str(), c.index))
            .collect();

        for row in &table.rows {
            // Create a resolved row with all columns
            let mut resolved_cells: Vec<ResolvedCell> = columns
                .iter()
                .map(|_| ResolvedCell::new(CellValue::Empty, table.source_path.clone()))
                .collect();

            // Fill in values from this row
            for (col_name, &unified_idx) in &col_index {
                if let Some(&table_idx) = table_col_map.get(col_name) {
                    if let Some(cell) = row.cells.get(table_idx) {
                        resolved_cells[unified_idx] =
                            ResolvedCell::new(cell.clone(), table.source_path.clone());
                    }
                }
            }

            match row.id {
                Some(id) => {
                    // Merge with existing row or insert new
                    if let Some(existing) = rows_by_id.get_mut(&id) {
                        // Override non-empty cells
                        for (i, new_cell) in resolved_cells.into_iter().enumerate() {
                            if !new_cell.value.is_empty() {
                                existing[i] = new_cell;
                            }
                        }
                    } else {
                        rows_by_id.insert(id, resolved_cells);
                    }
                }
                None => {
                    // No ID - append as separate row
                    rows_without_id.push((resolved_cells, table.source_path.clone()));
                }
            }
        }
    }

    // Convert to final row format
    let mut rows: Vec<ResolvedRow> = rows_by_id
        .into_iter()
        .map(|(id, cells)| ResolvedRow {
            id: Some(id),
            cells,
        })
        .collect();

    // Append rows without IDs
    for (cells, _source) in rows_without_id {
        rows.push(ResolvedRow { id: None, cells });
    }

    Ok(ResolvedTable {
        family_name: family_name.to_string(),
        columns,
        rows,
        sources,
    })
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::parser::parse_csv_str;

    #[test]
    fn test_merge_single_table() {
        let csv = "ID,Name,Value\n1,foo,100\n2,bar,200\n";
        let table = parse_csv_str(csv, "base.csv").unwrap();

        let result = merge_tables("test", vec![table]).unwrap();

        assert_eq!(result.columns.len(), 3);
        assert_eq!(result.rows.len(), 2);
        assert_eq!(result.rows[0].id, Some(1));
        assert_eq!(result.rows[1].id, Some(2));
    }

    #[test]
    fn test_merge_override_cells() {
        let base = "ID,Name,Value\n1,foo,100\n2,bar,200\n";
        let overlay = "ID,Name,Value\n1,FOO,999\n";

        let base_table = parse_csv_str(base, "base.csv").unwrap();
        let overlay_table = parse_csv_str(overlay, "overlay.csv").unwrap();

        let result = merge_tables("test", vec![base_table, overlay_table]).unwrap();

        // Row 1 should be overridden
        let row1 = result.find_row(1).unwrap();
        assert_eq!(row1.cells[1].value, CellValue::String("FOO".to_string()));
        assert_eq!(row1.cells[2].value, CellValue::Integer(999));

        // Row 2 should be unchanged
        let row2 = result.find_row(2).unwrap();
        assert_eq!(row2.cells[1].value, CellValue::String("bar".to_string()));
    }

    #[test]
    fn test_merge_add_new_row() {
        let base = "ID,Name\n1,foo\n";
        let overlay = "ID,Name\n2,bar\n";

        let base_table = parse_csv_str(base, "base.csv").unwrap();
        let overlay_table = parse_csv_str(overlay, "overlay.csv").unwrap();

        let result = merge_tables("test", vec![base_table, overlay_table]).unwrap();

        assert_eq!(result.rows.len(), 2);
        assert!(result.find_row(1).is_some());
        assert!(result.find_row(2).is_some());
    }

    #[test]
    fn test_merge_column_union() {
        let base = "ID,Name\n1,foo\n";
        let overlay = "ID,Extra\n1,bonus\n";

        let base_table = parse_csv_str(base, "base.csv").unwrap();
        let overlay_table = parse_csv_str(overlay, "overlay.csv").unwrap();

        let result = merge_tables("test", vec![base_table, overlay_table]).unwrap();

        // Should have 3 columns: ID, Name, Extra
        assert_eq!(result.columns.len(), 3);
        assert!(result.find_column("ID").is_some());
        assert!(result.find_column("Name").is_some());
        assert!(result.find_column("Extra").is_some());
    }

    #[test]
    fn test_provenance_tracking() {
        let base = "ID,Value\n1,100\n";
        let overlay = "ID,Value\n1,200\n";

        let base_table = parse_csv_str(base, "base.csv").unwrap();
        let overlay_table = parse_csv_str(overlay, "overlay.csv").unwrap();

        let result = merge_tables("test", vec![base_table, overlay_table]).unwrap();

        // Value should come from overlay
        let row = result.find_row(1).unwrap();
        assert_eq!(row.cells[1].source, PathBuf::from("overlay.csv"));
    }

    #[test]
    fn test_empty_cells_dont_override() {
        let base = "ID,Value\n1,100\n";
        let overlay = "ID,Value\n1,\n";

        let base_table = parse_csv_str(base, "base.csv").unwrap();
        let overlay_table = parse_csv_str(overlay, "overlay.csv").unwrap();

        let result = merge_tables("test", vec![base_table, overlay_table]).unwrap();

        // Empty cell should not override base value
        let row = result.find_row(1).unwrap();
        assert_eq!(row.cells[1].value, CellValue::Integer(100));
        assert_eq!(row.cells[1].source, PathBuf::from("base.csv"));
    }
}
