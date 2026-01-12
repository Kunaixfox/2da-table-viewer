//! CSV parser for 2DA table files

use crate::error::{Error, Result};
use crate::table::{CellValue, Column, Row, Table};
use std::fs::File;
use std::io::BufReader;
use std::path::Path;

/// Parse a CSV file into a Table
pub fn parse_csv<P: AsRef<Path>>(path: P) -> Result<Table> {
    let path = path.as_ref();
    let file = File::open(path).map_err(|e| Error::FileRead {
        path: path.to_path_buf(),
        source: e,
    })?;

    let reader = BufReader::new(file);
    let mut csv_reader = csv::ReaderBuilder::new()
        .has_headers(true)
        .flexible(true) // Allow varying number of fields
        .from_reader(reader);

    // Parse headers into columns
    let headers = csv_reader.headers().map_err(|e| Error::Csv {
        path: path.to_path_buf(),
        source: e,
    })?;

    let columns: Vec<Column> = headers
        .iter()
        .enumerate()
        .map(|(i, name)| Column::new(name.to_string(), i))
        .collect();

    if columns.is_empty() {
        return Err(Error::CsvParse {
            path: path.to_path_buf(),
            message: "no columns found in CSV".to_string(),
        });
    }

    // Parse rows
    let mut rows = Vec::new();
    for (row_idx, result) in csv_reader.records().enumerate() {
        let record = result.map_err(|e| Error::Csv {
            path: path.to_path_buf(),
            source: e,
        })?;

        let cells: Vec<CellValue> = record.iter().map(CellValue::parse).collect();

        // Extract ID from first column if it's an integer
        let id = cells.first().and_then(|c| match c {
            CellValue::Integer(i) => Some(*i),
            _ => None,
        });

        // Pad with empty cells if row is shorter than header
        let mut padded_cells = cells;
        while padded_cells.len() < columns.len() {
            padded_cells.push(CellValue::Empty);
        }

        // Warn if row is longer than header (truncate)
        if padded_cells.len() > columns.len() {
            eprintln!(
                "Warning: row {} in {} has more cells than columns, truncating",
                row_idx + 1,
                path.display()
            );
            padded_cells.truncate(columns.len());
        }

        rows.push(Row::new(id, padded_cells));
    }

    Ok(Table {
        columns,
        rows,
        source_path: path.to_path_buf(),
    })
}

/// Parse CSV from a string (useful for testing)
pub fn parse_csv_str(content: &str, source_name: &str) -> Result<Table> {
    let mut csv_reader = csv::ReaderBuilder::new()
        .has_headers(true)
        .flexible(true)
        .from_reader(content.as_bytes());

    let path = std::path::PathBuf::from(source_name);

    // Parse headers into columns
    let headers = csv_reader.headers().map_err(|e| Error::Csv {
        path: path.clone(),
        source: e,
    })?;

    let columns: Vec<Column> = headers
        .iter()
        .enumerate()
        .map(|(i, name)| Column::new(name.to_string(), i))
        .collect();

    if columns.is_empty() {
        return Err(Error::CsvParse {
            path: path.clone(),
            message: "no columns found in CSV".to_string(),
        });
    }

    // Parse rows
    let mut rows = Vec::new();
    for result in csv_reader.records() {
        let record = result.map_err(|e| Error::Csv {
            path: path.clone(),
            source: e,
        })?;

        let cells: Vec<CellValue> = record.iter().map(CellValue::parse).collect();

        let id = cells.first().and_then(|c| match c {
            CellValue::Integer(i) => Some(*i),
            _ => None,
        });

        let mut padded_cells = cells;
        while padded_cells.len() < columns.len() {
            padded_cells.push(CellValue::Empty);
        }
        if padded_cells.len() > columns.len() {
            padded_cells.truncate(columns.len());
        }

        rows.push(Row::new(id, padded_cells));
    }

    Ok(Table {
        columns,
        rows,
        source_path: path,
    })
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_parse_simple_csv() {
        let csv = "ID,Name,Value\n1,foo,100\n2,bar,200\n";
        let table = parse_csv_str(csv, "test.csv").unwrap();

        assert_eq!(table.columns.len(), 3);
        assert_eq!(table.columns[0].name, "ID");
        assert_eq!(table.columns[1].name, "Name");
        assert_eq!(table.columns[2].name, "Value");

        assert_eq!(table.rows.len(), 2);
        assert_eq!(table.rows[0].id, Some(1));
        assert_eq!(table.rows[1].id, Some(2));
    }

    #[test]
    fn test_parse_with_empty_cells() {
        let csv = "ID,Name,Value\n1,,100\n2,bar,\n";
        let table = parse_csv_str(csv, "test.csv").unwrap();

        assert_eq!(table.rows[0].cells[1], CellValue::Empty);
        assert_eq!(table.rows[1].cells[2], CellValue::Empty);
    }

    #[test]
    fn test_parse_hex_column_names() {
        let csv = "ID,0xABCD,0x1234\n1,10,20\n";
        let table = parse_csv_str(csv, "test.csv").unwrap();

        assert_eq!(table.columns[1].name, "0xABCD");
        assert_eq!(table.columns[2].name, "0x1234");
    }

    #[test]
    fn test_parse_with_floats() {
        let csv = "ID,Value\n1,3.14\n2,-2.5\n";
        let table = parse_csv_str(csv, "test.csv").unwrap();

        assert_eq!(table.rows[0].cells[1], CellValue::Float(3.14));
        assert_eq!(table.rows[1].cells[1], CellValue::Float(-2.5));
    }

    #[test]
    fn test_parse_non_integer_id() {
        let csv = "Name,Value\nfoo,100\nbar,200\n";
        let table = parse_csv_str(csv, "test.csv").unwrap();

        // No integer in first column, so id should be None
        assert_eq!(table.rows[0].id, None);
        assert_eq!(table.rows[1].id, None);
    }
}
