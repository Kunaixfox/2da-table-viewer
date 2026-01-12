//! Core table types for representing 2DA data

use serde::{Deserialize, Serialize};
use std::path::PathBuf;

/// A parsed table from a single CSV file
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Table {
    /// Column definitions
    pub columns: Vec<Column>,
    /// Row data
    pub rows: Vec<Row>,
    /// Source file path
    pub source_path: PathBuf,
}

impl Table {
    /// Create a new empty table
    pub fn new(source_path: PathBuf) -> Self {
        Self {
            columns: Vec::new(),
            rows: Vec::new(),
            source_path,
        }
    }

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

    /// Find a row by ID (assumes first column is ID)
    pub fn find_row(&self, id: i64) -> Option<&Row> {
        self.rows.iter().find(|r| r.id == Some(id))
    }
}

/// A column definition
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Column {
    /// Column name (e.g., "ID" or "0xC4FDA9ED")
    pub name: String,
    /// Column index (0-based)
    pub index: usize,
}

impl Column {
    /// Create a new column
    pub fn new(name: String, index: usize) -> Self {
        Self { name, index }
    }
}

/// A row of data
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Row {
    /// Row ID (from first column, if it's numeric)
    pub id: Option<i64>,
    /// Cell values for each column
    pub cells: Vec<CellValue>,
}

impl Row {
    /// Create a new row
    pub fn new(id: Option<i64>, cells: Vec<CellValue>) -> Self {
        Self { id, cells }
    }

    /// Get a cell value by column index
    pub fn get(&self, index: usize) -> Option<&CellValue> {
        self.cells.get(index)
    }
}

/// A cell value with type detection
#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub enum CellValue {
    /// Integer value
    Integer(i64),
    /// Floating-point value
    Float(f64),
    /// String value
    String(String),
    /// Empty/null cell
    Empty,
}

impl CellValue {
    /// Parse a string into a CellValue, detecting the type
    pub fn parse(s: &str) -> Self {
        let trimmed = s.trim();

        if trimmed.is_empty() {
            return CellValue::Empty;
        }

        // Try parsing as integer first
        if let Ok(i) = trimmed.parse::<i64>() {
            return CellValue::Integer(i);
        }

        // Try parsing as float
        if let Ok(f) = trimmed.parse::<f64>() {
            return CellValue::Float(f);
        }

        // Otherwise, keep as string
        CellValue::String(trimmed.to_string())
    }

    /// Check if the cell is empty
    pub fn is_empty(&self) -> bool {
        matches!(self, CellValue::Empty)
    }

    /// Convert to a display string
    pub fn to_string_value(&self) -> String {
        match self {
            CellValue::Integer(i) => i.to_string(),
            CellValue::Float(f) => f.to_string(),
            CellValue::String(s) => s.clone(),
            CellValue::Empty => String::new(),
        }
    }
}

impl std::fmt::Display for CellValue {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            CellValue::Integer(i) => write!(f, "{}", i),
            CellValue::Float(fl) => write!(f, "{}", fl),
            CellValue::String(s) => write!(f, "{}", s),
            CellValue::Empty => write!(f, ""),
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_cell_value_parse_integer() {
        assert_eq!(CellValue::parse("42"), CellValue::Integer(42));
        assert_eq!(CellValue::parse("-123"), CellValue::Integer(-123));
        assert_eq!(CellValue::parse("0"), CellValue::Integer(0));
    }

    #[test]
    fn test_cell_value_parse_float() {
        assert_eq!(CellValue::parse("3.14"), CellValue::Float(3.14));
        assert_eq!(CellValue::parse("-2.5"), CellValue::Float(-2.5));
    }

    #[test]
    fn test_cell_value_parse_string() {
        assert_eq!(
            CellValue::parse("hello"),
            CellValue::String("hello".to_string())
        );
        assert_eq!(
            CellValue::parse("0xABCD"),
            CellValue::String("0xABCD".to_string())
        );
    }

    #[test]
    fn test_cell_value_parse_empty() {
        assert_eq!(CellValue::parse(""), CellValue::Empty);
        assert_eq!(CellValue::parse("   "), CellValue::Empty);
    }

    #[test]
    fn test_cell_value_is_empty() {
        assert!(CellValue::Empty.is_empty());
        assert!(!CellValue::Integer(0).is_empty());
        assert!(!CellValue::String("".to_string()).is_empty());
    }
}
