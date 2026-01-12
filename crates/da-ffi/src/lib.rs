//! C FFI bindings for da-core
//!
//! This crate provides a C-compatible API for use with Qt or other C/C++ applications.
//! Currently a stub - will be implemented when Qt UI integration begins.

use std::ffi::{CStr, CString};
use std::os::raw::c_char;
use std::path::PathBuf;
use std::ptr;

/// Opaque handle to a scan result
pub struct FfiScanResult {
    inner: da_core::scanner::ScanResult,
}

/// Opaque handle to a resolved table
pub struct FfiResolvedTable {
    inner: da_core::ResolvedTable,
}

/// Scan directories for CSV files
///
/// # Safety
/// - `roots` must be a valid pointer to an array of C strings
/// - `count` must be the correct length of the array
/// - Returns null on error
#[no_mangle]
pub unsafe extern "C" fn da_scan_roots(roots: *const *const c_char, count: usize) -> *mut FfiScanResult {
    if roots.is_null() || count == 0 {
        return ptr::null_mut();
    }

    let root_paths: Vec<PathBuf> = (0..count)
        .filter_map(|i| {
            let ptr = *roots.add(i);
            if ptr.is_null() {
                None
            } else {
                CStr::from_ptr(ptr).to_str().ok().map(PathBuf::from)
            }
        })
        .collect();

    match da_core::scan_directory(&root_paths) {
        Ok(result) => Box::into_raw(Box::new(FfiScanResult { inner: result })),
        Err(_) => ptr::null_mut(),
    }
}

/// Free a scan result
///
/// # Safety
/// - `result` must be a valid pointer returned by `da_scan_roots` or null
#[no_mangle]
pub unsafe extern "C" fn da_free_scan_result(result: *mut FfiScanResult) {
    if !result.is_null() {
        drop(Box::from_raw(result));
    }
}

/// Get the number of families in a scan result
///
/// # Safety
/// - `result` must be a valid pointer returned by `da_scan_roots`
#[no_mangle]
pub unsafe extern "C" fn da_scan_family_count(result: *const FfiScanResult) -> usize {
    if result.is_null() {
        return 0;
    }
    (*result).inner.families.len()
}

/// Get a family name by index
///
/// # Safety
/// - `result` must be a valid pointer returned by `da_scan_roots`
/// - Returns null if index is out of bounds
/// - Caller must free the returned string with `da_free_string`
#[no_mangle]
pub unsafe extern "C" fn da_scan_family_name(result: *const FfiScanResult, index: usize) -> *mut c_char {
    if result.is_null() {
        return ptr::null_mut();
    }

    (*result)
        .inner
        .families
        .get(index)
        .and_then(|f| CString::new(f.name.as_str()).ok())
        .map(|s| s.into_raw())
        .unwrap_or(ptr::null_mut())
}

/// Merge a family into a resolved table
///
/// # Safety
/// - `result` must be a valid pointer returned by `da_scan_roots`
/// - `family_name` must be a valid C string
/// - Returns null on error
#[no_mangle]
pub unsafe extern "C" fn da_merge_family(
    result: *const FfiScanResult,
    family_name: *const c_char,
) -> *mut FfiResolvedTable {
    if result.is_null() || family_name.is_null() {
        return ptr::null_mut();
    }

    let name = match CStr::from_ptr(family_name).to_str() {
        Ok(s) => s,
        Err(_) => return ptr::null_mut(),
    };

    let family = match (*result).inner.find_family(name) {
        Some(f) => f,
        None => return ptr::null_mut(),
    };

    match da_core::merge_family(family) {
        Ok(table) => Box::into_raw(Box::new(FfiResolvedTable { inner: table })),
        Err(_) => ptr::null_mut(),
    }
}

/// Free a resolved table
///
/// # Safety
/// - `table` must be a valid pointer returned by `da_merge_family` or null
#[no_mangle]
pub unsafe extern "C" fn da_free_resolved_table(table: *mut FfiResolvedTable) {
    if !table.is_null() {
        drop(Box::from_raw(table));
    }
}

/// Get the row count of a resolved table
///
/// # Safety
/// - `table` must be a valid pointer returned by `da_merge_family`
#[no_mangle]
pub unsafe extern "C" fn da_table_row_count(table: *const FfiResolvedTable) -> usize {
    if table.is_null() {
        return 0;
    }
    (*table).inner.row_count()
}

/// Get the column count of a resolved table
///
/// # Safety
/// - `table` must be a valid pointer returned by `da_merge_family`
#[no_mangle]
pub unsafe extern "C" fn da_table_col_count(table: *const FfiResolvedTable) -> usize {
    if table.is_null() {
        return 0;
    }
    (*table).inner.column_count()
}

/// Get a column name by index
///
/// # Safety
/// - `table` must be a valid pointer returned by `da_merge_family`
/// - Returns null if index is out of bounds
/// - Caller must free the returned string with `da_free_string`
#[no_mangle]
pub unsafe extern "C" fn da_table_col_name(table: *const FfiResolvedTable, index: usize) -> *mut c_char {
    if table.is_null() {
        return ptr::null_mut();
    }

    (*table)
        .inner
        .columns
        .get(index)
        .and_then(|c| CString::new(c.name.as_str()).ok())
        .map(|s| s.into_raw())
        .unwrap_or(ptr::null_mut())
}

/// Get a cell value as a string
///
/// # Safety
/// - `table` must be a valid pointer returned by `da_merge_family`
/// - Returns null if row or col is out of bounds
/// - Caller must free the returned string with `da_free_string`
#[no_mangle]
pub unsafe extern "C" fn da_table_cell(
    table: *const FfiResolvedTable,
    row: usize,
    col: usize,
) -> *mut c_char {
    if table.is_null() {
        return ptr::null_mut();
    }

    (*table)
        .inner
        .rows
        .get(row)
        .and_then(|r| r.cells.get(col))
        .and_then(|c| CString::new(c.value.to_string_value()).ok())
        .map(|s| s.into_raw())
        .unwrap_or(ptr::null_mut())
}

/// Get the provenance (source file) for a cell
///
/// # Safety
/// - `table` must be a valid pointer returned by `da_merge_family`
/// - Returns null if row or col is out of bounds
/// - Caller must free the returned string with `da_free_string`
#[no_mangle]
pub unsafe extern "C" fn da_table_provenance(
    table: *const FfiResolvedTable,
    row: usize,
    col: usize,
) -> *mut c_char {
    if table.is_null() {
        return ptr::null_mut();
    }

    (*table)
        .inner
        .rows
        .get(row)
        .and_then(|r| r.cells.get(col))
        .and_then(|c| c.source.to_str())
        .and_then(|s| CString::new(s).ok())
        .map(|s| s.into_raw())
        .unwrap_or(ptr::null_mut())
}

/// Free a string returned by other FFI functions
///
/// # Safety
/// - `s` must be a valid pointer returned by a da_* function or null
#[no_mangle]
pub unsafe extern "C" fn da_free_string(s: *mut c_char) {
    if !s.is_null() {
        drop(CString::from_raw(s));
    }
}
