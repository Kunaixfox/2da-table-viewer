//! C FFI bindings for da-core
//!
//! This crate provides a C-compatible API for use with Qt or other C/C++ applications.

use std::cell::RefCell;
use std::ffi::{CStr, CString};
use std::os::raw::c_char;
use std::panic::{catch_unwind, AssertUnwindSafe};
use std::path::PathBuf;
use std::ptr;

use da_core::{
    merge_family, scan_directory, CellValue, Family, HistoryEntry, HistoryFile, PatchFile,
    ResolvedTable,
};

// Thread-local error storage
thread_local! {
    static LAST_ERROR: RefCell<Option<CString>> = const { RefCell::new(None) };
}

fn set_error(msg: &str) {
    LAST_ERROR.with(|e| {
        *e.borrow_mut() = CString::new(msg).ok();
    });
}

fn clear_error() {
    LAST_ERROR.with(|e| {
        *e.borrow_mut() = None;
    });
}

// ============================================================================
// Opaque Handle Types
// ============================================================================

/// Opaque handle to a scan result
pub struct FfiScanResult {
    families: Vec<Family>,
}

/// Opaque handle to a resolved table
pub struct FfiResolvedTable {
    inner: ResolvedTable,
}

/// Opaque handle to a patch result
pub struct FfiPatchResult {
    exported_files: Vec<PathBuf>,
}

/// Opaque handle to a history file
pub struct FfiHistoryFile {
    #[allow(dead_code)]
    inner: HistoryFile,
    // Flattened entries for indexed access
    entries: Vec<HistoryEntry>,
}

// ============================================================================
// FFI Struct Types (must match da_ffi.h)
// ============================================================================

#[repr(C)]
pub struct FfiStringResult {
    pub data: *mut c_char,
    pub len: usize,
    pub success: i32,
}

#[repr(C)]
pub struct FfiFamilyInfo {
    pub name: *mut c_char,
    pub member_count: usize,
}

#[repr(C)]
pub struct FfiMemberInfo {
    pub path: *mut c_char,
    pub suffix: *mut c_char,
    pub is_base: i32,
}

#[repr(C)]
pub struct FfiCellValue {
    pub value_type: i32, // 0=Empty, 1=Integer, 2=Float, 3=String
    pub int_value: i64,
    pub float_value: f64,
    pub string_value: *mut c_char,
}

#[repr(C)]
pub struct FfiResolvedCell {
    pub value: FfiCellValue,
    pub source_path: *mut c_char,
}

#[repr(C)]
pub struct FfiColumnInfo {
    pub name: *mut c_char,
    pub index: usize,
}

#[repr(C)]
pub struct FfiHistoryEntry {
    pub family: *mut c_char,
    pub timestamp: *mut c_char,
    pub edit_count: usize,
    pub patch_file: *mut c_char,
}

// ============================================================================
// Helper Functions
// ============================================================================

fn to_c_string(s: &str) -> *mut c_char {
    CString::new(s)
        .map(|cs| cs.into_raw())
        .unwrap_or(ptr::null_mut())
}

fn from_c_str(ptr: *const c_char) -> Option<String> {
    if ptr.is_null() {
        None
    } else {
        unsafe { CStr::from_ptr(ptr).to_str().ok().map(String::from) }
    }
}

// ============================================================================
// Error Handling
// ============================================================================

/// Get last error message (thread-local)
#[no_mangle]
pub extern "C" fn ffi_last_error() -> *const c_char {
    LAST_ERROR.with(|e| {
        e.borrow()
            .as_ref()
            .map(|s| s.as_ptr())
            .unwrap_or(ptr::null())
    })
}

/// Clear last error
#[no_mangle]
pub extern "C" fn ffi_clear_error() {
    clear_error();
}

// ============================================================================
// Scanning and Family Operations
// ============================================================================

/// Scan a directory for CSV files and group into families
#[no_mangle]
pub unsafe extern "C" fn ffi_scan_directory(root_path: *const c_char) -> *mut FfiScanResult {
    clear_error();

    let path = match from_c_str(root_path) {
        Some(p) => p,
        None => {
            set_error("Invalid path");
            return ptr::null_mut();
        }
    };

    match scan_directory(&[PathBuf::from(&path)]) {
        Ok(result) => Box::into_raw(Box::new(FfiScanResult {
            families: result.families,
        })),
        Err(e) => {
            set_error(&e.to_string());
            ptr::null_mut()
        }
    }
}

/// Get number of families in scan result
#[no_mangle]
pub unsafe extern "C" fn ffi_scan_family_count(result: *const FfiScanResult) -> usize {
    if result.is_null() {
        return 0;
    }
    (*result).families.len()
}

/// Get family info by index
#[no_mangle]
pub unsafe extern "C" fn ffi_scan_get_family(
    result: *const FfiScanResult,
    index: usize,
) -> *mut FfiFamilyInfo {
    if result.is_null() {
        return ptr::null_mut();
    }

    match (*result).families.get(index) {
        Some(family) => {
            let info = Box::new(FfiFamilyInfo {
                name: to_c_string(&family.name),
                member_count: family.members.len(),
            });
            Box::into_raw(info)
        }
        None => ptr::null_mut(),
    }
}

/// Get members of a family by family name
#[no_mangle]
pub unsafe extern "C" fn ffi_scan_get_members(
    result: *const FfiScanResult,
    family_name: *const c_char,
    out_count: *mut usize,
) -> *mut FfiMemberInfo {
    if result.is_null() || family_name.is_null() || out_count.is_null() {
        return ptr::null_mut();
    }

    let name = match from_c_str(family_name) {
        Some(n) => n,
        None => return ptr::null_mut(),
    };

    let family = match (*result).families.iter().find(|f| f.name == name) {
        Some(f) => f,
        None => return ptr::null_mut(),
    };

    let members: Vec<FfiMemberInfo> = family
        .members
        .iter()
        .map(|m| FfiMemberInfo {
            path: to_c_string(m.path.to_string_lossy().as_ref()),
            suffix: m
                .suffix
                .as_ref()
                .map(|s| to_c_string(s))
                .unwrap_or(ptr::null_mut()),
            is_base: if m.suffix.is_none() { 1 } else { 0 },
        })
        .collect();

    *out_count = members.len();

    if members.is_empty() {
        ptr::null_mut()
    } else {
        let boxed = members.into_boxed_slice();
        Box::into_raw(boxed) as *mut FfiMemberInfo
    }
}

/// Search families by name pattern (case-insensitive substring)
#[no_mangle]
pub unsafe extern "C" fn ffi_search_families(
    result: *const FfiScanResult,
    pattern: *const c_char,
    out_count: *mut usize,
) -> *mut *mut c_char {
    if result.is_null() || pattern.is_null() || out_count.is_null() {
        return ptr::null_mut();
    }

    let pattern_str = match from_c_str(pattern) {
        Some(p) => p.to_lowercase(),
        None => return ptr::null_mut(),
    };

    let matches: Vec<*mut c_char> = (*result)
        .families
        .iter()
        .filter(|f| f.name.to_lowercase().contains(&pattern_str))
        .map(|f| to_c_string(&f.name))
        .collect();

    *out_count = matches.len();

    if matches.is_empty() {
        ptr::null_mut()
    } else {
        let boxed = matches.into_boxed_slice();
        Box::into_raw(boxed) as *mut *mut c_char
    }
}

/// Free scan result
#[no_mangle]
pub unsafe extern "C" fn ffi_scan_free(result: *mut FfiScanResult) {
    if !result.is_null() {
        drop(Box::from_raw(result));
    }
}

// ============================================================================
// Table Operations
// ============================================================================

/// Merge a family into a resolved table
#[no_mangle]
pub unsafe extern "C" fn ffi_merge_family(
    scan_result: *const FfiScanResult,
    family_name: *const c_char,
) -> *mut FfiResolvedTable {
    clear_error();

    if scan_result.is_null() || family_name.is_null() {
        set_error("Null pointer");
        return ptr::null_mut();
    }

    let name = match from_c_str(family_name) {
        Some(n) => n,
        None => {
            set_error("Invalid family name");
            return ptr::null_mut();
        }
    };

    let family = match (*scan_result).families.iter().find(|f| f.name == name) {
        Some(f) => f,
        None => {
            set_error(&format!("Family not found: {}", name));
            return ptr::null_mut();
        }
    };

    match merge_family(family) {
        Ok(table) => Box::into_raw(Box::new(FfiResolvedTable { inner: table })),
        Err(e) => {
            set_error(&e.to_string());
            ptr::null_mut()
        }
    }
}

/// Get column count
#[no_mangle]
pub unsafe extern "C" fn ffi_table_column_count(table: *const FfiResolvedTable) -> usize {
    if table.is_null() {
        return 0;
    }
    (*table).inner.columns.len()
}

/// Get row count
#[no_mangle]
pub unsafe extern "C" fn ffi_table_row_count(table: *const FfiResolvedTable) -> usize {
    if table.is_null() {
        return 0;
    }
    (*table).inner.rows.len()
}

/// Get column info by index
#[no_mangle]
pub unsafe extern "C" fn ffi_table_get_column(
    table: *const FfiResolvedTable,
    index: usize,
) -> *mut FfiColumnInfo {
    if table.is_null() {
        return ptr::null_mut();
    }

    match (*table).inner.columns.get(index) {
        Some(col) => {
            let info = Box::new(FfiColumnInfo {
                name: to_c_string(&col.name),
                index: col.index,
            });
            Box::into_raw(info)
        }
        None => ptr::null_mut(),
    }
}

/// Get cell at row/column
#[no_mangle]
pub unsafe extern "C" fn ffi_table_get_cell(
    table: *const FfiResolvedTable,
    row_index: usize,
    col_index: usize,
) -> *mut FfiResolvedCell {
    if table.is_null() {
        return ptr::null_mut();
    }

    let row = match (*table).inner.rows.get(row_index) {
        Some(r) => r,
        None => return ptr::null_mut(),
    };

    let cell = match row.cells.get(col_index) {
        Some(c) => c,
        None => return ptr::null_mut(),
    };

    let ffi_value = match &cell.value {
        CellValue::Empty => FfiCellValue {
            value_type: 0,
            int_value: 0,
            float_value: 0.0,
            string_value: ptr::null_mut(),
        },
        CellValue::Integer(i) => FfiCellValue {
            value_type: 1,
            int_value: *i,
            float_value: 0.0,
            string_value: ptr::null_mut(),
        },
        CellValue::Float(f) => FfiCellValue {
            value_type: 2,
            int_value: 0,
            float_value: *f,
            string_value: ptr::null_mut(),
        },
        CellValue::String(s) => FfiCellValue {
            value_type: 3,
            int_value: 0,
            float_value: 0.0,
            string_value: to_c_string(s),
        },
    };

    let ffi_cell = Box::new(FfiResolvedCell {
        value: ffi_value,
        source_path: to_c_string(cell.source.to_string_lossy().as_ref()),
    });

    Box::into_raw(ffi_cell)
}

/// Get row ID for a given row index
#[no_mangle]
pub unsafe extern "C" fn ffi_table_get_row_id(
    table: *const FfiResolvedTable,
    row_index: usize,
) -> i64 {
    if table.is_null() {
        return -1;
    }

    match (*table).inner.rows.get(row_index) {
        Some(row) => row.id.unwrap_or(-1),
        None => -1,
    }
}

/// Filter rows by column value (case-insensitive substring)
#[no_mangle]
pub unsafe extern "C" fn ffi_table_filter_rows(
    table: *const FfiResolvedTable,
    column_name: *const c_char,
    value_pattern: *const c_char,
    out_count: *mut usize,
) -> *mut usize {
    if table.is_null() || column_name.is_null() || value_pattern.is_null() || out_count.is_null() {
        return ptr::null_mut();
    }

    let col_name = match from_c_str(column_name) {
        Some(n) => n,
        None => return ptr::null_mut(),
    };

    let pattern = match from_c_str(value_pattern) {
        Some(p) => p.to_lowercase(),
        None => return ptr::null_mut(),
    };

    // Find column index
    let col_idx = match (*table).inner.columns.iter().position(|c| c.name == col_name) {
        Some(idx) => idx,
        None => return ptr::null_mut(),
    };

    // Find matching rows
    let matches: Vec<usize> = (*table)
        .inner
        .rows
        .iter()
        .enumerate()
        .filter(|(_, row)| {
            row.cells.get(col_idx).map_or(false, |cell| {
                cell.value
                    .to_string_value()
                    .to_lowercase()
                    .contains(&pattern)
            })
        })
        .map(|(idx, _)| idx)
        .collect();

    *out_count = matches.len();

    if matches.is_empty() {
        ptr::null_mut()
    } else {
        let boxed = matches.into_boxed_slice();
        Box::into_raw(boxed) as *mut usize
    }
}

/// Free resolved table
#[no_mangle]
pub unsafe extern "C" fn ffi_table_free(table: *mut FfiResolvedTable) {
    if !table.is_null() {
        drop(Box::from_raw(table));
    }
}

// ============================================================================
// Patch Operations
// ============================================================================

/// Create a new patch (returns JSON string)
#[no_mangle]
pub unsafe extern "C" fn ffi_create_patch(family_name: *const c_char) -> FfiStringResult {
    let name = match from_c_str(family_name) {
        Some(n) => n,
        None => {
            return FfiStringResult {
                data: ptr::null_mut(),
                len: 0,
                success: 0,
            }
        }
    };

    let patch = PatchFile {
        family: name,
        edits: vec![],
    };

    match serde_json::to_string_pretty(&patch) {
        Ok(json) => {
            let len = json.len();
            FfiStringResult {
                data: to_c_string(&json),
                len,
                success: 1,
            }
        }
        Err(e) => {
            let err = e.to_string();
            let len = err.len();
            FfiStringResult {
                data: to_c_string(&err),
                len,
                success: 0,
            }
        }
    }
}

/// Apply a patch and export modified files
#[no_mangle]
pub unsafe extern "C" fn ffi_apply_patch(
    scan_result: *const FfiScanResult,
    patch_json: *const c_char,
    output_dir: *const c_char,
    history_path: *const c_char,
) -> *mut FfiPatchResult {
    clear_error();

    // Wrap in catch_unwind to prevent panics from crashing the app
    let result = catch_unwind(AssertUnwindSafe(|| {
        if scan_result.is_null() || patch_json.is_null() || output_dir.is_null() {
            set_error("Null pointer");
            return ptr::null_mut();
        }

        let json = match from_c_str(patch_json) {
            Some(j) => j,
            None => {
                set_error("Invalid patch JSON");
                return ptr::null_mut();
            }
        };

        let out_path = match from_c_str(output_dir) {
            Some(p) => PathBuf::from(p),
            None => {
                set_error("Invalid output directory");
                return ptr::null_mut();
            }
        };

        let history_file_path = from_c_str(history_path).map(PathBuf::from);

        let patch: PatchFile = match serde_json::from_str(&json) {
            Ok(p) => p,
            Err(e) => {
                set_error(&format!("Invalid patch format: {}", e));
                return ptr::null_mut();
            }
        };

        let family = match (*scan_result)
            .families
            .iter()
            .find(|f| f.name == patch.family)
        {
            Some(f) => f,
            None => {
                set_error(&format!("Family not found: {}", patch.family));
                return ptr::null_mut();
            }
        };

        // First merge the family to get the resolved table
        let table = match merge_family(family) {
            Ok(t) => t,
            Err(e) => {
                set_error(&format!("Failed to merge family: {}", e));
                return ptr::null_mut();
            }
        };

        // Export with edits
        match da_core::export_with_edits(&table, &patch, &out_path) {
            Ok(result) => {
                // Save to history if path provided
                if let Some(hist_path) = history_file_path {
                    let entry = da_core::create_history_entry(
                        &patch,
                        result.files_written.clone(),
                        out_path,
                    );
                    if let Ok(mut history) = HistoryFile::load(&hist_path) {
                        history.add_entry(entry);
                        let _ = history.save(&hist_path);
                    }
                }

                Box::into_raw(Box::new(FfiPatchResult {
                    exported_files: result.files_written,
                }))
            }
            Err(e) => {
                set_error(&e.to_string());
                ptr::null_mut()
            }
        }
    }));

    match result {
        Ok(ptr) => ptr,
        Err(_) => {
            set_error("Internal error: panic occurred in apply_patch");
            ptr::null_mut()
        }
    }
}

/// Validate a patch without applying
#[no_mangle]
pub unsafe extern "C" fn ffi_validate_patch(
    scan_result: *const FfiScanResult,
    patch_json: *const c_char,
) -> FfiStringResult {
    // Wrap in catch_unwind to prevent panics from crashing the app
    let result = catch_unwind(AssertUnwindSafe(|| {
        if scan_result.is_null() || patch_json.is_null() {
            return FfiStringResult {
                data: to_c_string("Null pointer"),
                len: 12,
                success: 0,
            };
        }

        let json = match from_c_str(patch_json) {
            Some(j) => j,
            None => {
                return FfiStringResult {
                    data: to_c_string("Invalid JSON string"),
                    len: 19,
                    success: 0,
                }
            }
        };

        let patch: PatchFile = match serde_json::from_str(&json) {
            Ok(p) => p,
            Err(e) => {
                let msg = format!("Invalid patch format: {}", e);
                let len = msg.len();
                return FfiStringResult {
                    data: to_c_string(&msg),
                    len,
                    success: 0,
                };
            }
        };

        // Check family exists
        let family = match (*scan_result)
            .families
            .iter()
            .find(|f| f.name == patch.family)
        {
            Some(f) => f,
            None => {
                let msg = format!("Family not found: {}", patch.family);
                let len = msg.len();
                return FfiStringResult {
                    data: to_c_string(&msg),
                    len,
                    success: 0,
                };
            }
        };

        // Merge to validate edits
        let table = match merge_family(family) {
            Ok(t) => t,
            Err(e) => {
                let msg = format!("Failed to merge family: {}", e);
                let len = msg.len();
                return FfiStringResult {
                    data: to_c_string(&msg),
                    len,
                    success: 0,
                };
            }
        };

        // Validate each edit
        for edit in &patch.edits {
            // Check column exists
            if !table.columns.iter().any(|c| c.name == edit.column) {
                let msg = format!("Column not found: {}", edit.column);
                let len = msg.len();
                return FfiStringResult {
                    data: to_c_string(&msg),
                    len,
                    success: 0,
                };
            }

            // Check row exists
            if !table.rows.iter().any(|r| r.id == Some(edit.row_id)) {
                let msg = format!("Row not found: {}", edit.row_id);
                let len = msg.len();
                return FfiStringResult {
                    data: to_c_string(&msg),
                    len,
                    success: 0,
                };
            }
        }

        // Valid
        FfiStringResult {
            data: ptr::null_mut(),
            len: 0,
            success: 1,
        }
    }));

    match result {
        Ok(r) => r,
        Err(_) => FfiStringResult {
            data: to_c_string("Internal error: panic occurred in validate_patch"),
            len: 47,
            success: 0,
        },
    }
}

/// Get number of files exported from patch result
#[no_mangle]
pub unsafe extern "C" fn ffi_patch_export_count(result: *const FfiPatchResult) -> usize {
    if result.is_null() {
        return 0;
    }
    (*result).exported_files.len()
}

/// Get exported file path by index
#[no_mangle]
pub unsafe extern "C" fn ffi_patch_get_export_path(
    result: *const FfiPatchResult,
    index: usize,
) -> *mut c_char {
    if result.is_null() {
        return ptr::null_mut();
    }

    (*result)
        .exported_files
        .get(index)
        .map(|p| to_c_string(p.to_string_lossy().as_ref()))
        .unwrap_or(ptr::null_mut())
}

/// Free patch result
#[no_mangle]
pub unsafe extern "C" fn ffi_patch_free(result: *mut FfiPatchResult) {
    if !result.is_null() {
        drop(Box::from_raw(result));
    }
}

// ============================================================================
// History Operations
// ============================================================================

/// Load history file
#[no_mangle]
pub unsafe extern "C" fn ffi_history_load(path: *const c_char) -> *mut FfiHistoryFile {
    let path_str = match from_c_str(path) {
        Some(p) => p,
        None => return ptr::null_mut(),
    };

    match HistoryFile::load(&PathBuf::from(path_str)) {
        Ok(history) => {
            // Flatten entries for indexed access, sorted by timestamp (most recent first)
            let mut entries: Vec<HistoryEntry> = history
                .entries
                .values()
                .flatten()
                .cloned()
                .collect();
            entries.sort_by(|a, b| b.timestamp.cmp(&a.timestamp));

            Box::into_raw(Box::new(FfiHistoryFile { inner: history, entries }))
        }
        Err(_) => {
            // Return empty history if file doesn't exist
            Box::into_raw(Box::new(FfiHistoryFile {
                inner: HistoryFile::new(),
                entries: vec![],
            }))
        }
    }
}

/// Get history entry count
#[no_mangle]
pub unsafe extern "C" fn ffi_history_count(history: *const FfiHistoryFile) -> usize {
    if history.is_null() {
        return 0;
    }
    (*history).entries.len()
}

/// Get history entry by index (already sorted most recent first)
#[no_mangle]
pub unsafe extern "C" fn ffi_history_get_entry(
    history: *const FfiHistoryFile,
    index: usize,
) -> *mut FfiHistoryEntry {
    if history.is_null() {
        return ptr::null_mut();
    }

    let entry = match (*history).entries.get(index) {
        Some(e) => e,
        None => return ptr::null_mut(),
    };

    let ffi_entry = Box::new(FfiHistoryEntry {
        family: to_c_string(&entry.family),
        timestamp: to_c_string(&entry.timestamp.to_rfc3339()),
        edit_count: entry.patch.edits.len(),
        patch_file: ptr::null_mut(), // We don't store patch file path separately
    });

    Box::into_raw(ffi_entry)
}

/// Free history file
#[no_mangle]
pub unsafe extern "C" fn ffi_history_free(history: *mut FfiHistoryFile) {
    if !history.is_null() {
        drop(Box::from_raw(history));
    }
}

// ============================================================================
// Memory Management
// ============================================================================

#[no_mangle]
pub unsafe extern "C" fn ffi_free_string(s: *mut c_char) {
    if !s.is_null() {
        drop(CString::from_raw(s));
    }
}

#[no_mangle]
pub unsafe extern "C" fn ffi_free_string_array(arr: *mut *mut c_char, count: usize) {
    if !arr.is_null() {
        let slice = std::slice::from_raw_parts_mut(arr, count);
        for s in slice.iter() {
            if !s.is_null() {
                drop(CString::from_raw(*s));
            }
        }
        drop(Box::from_raw(std::slice::from_raw_parts_mut(arr, count) as *mut [*mut c_char]));
    }
}

#[no_mangle]
pub unsafe extern "C" fn ffi_free_family_info(info: *mut FfiFamilyInfo) {
    if !info.is_null() {
        let info = Box::from_raw(info);
        if !info.name.is_null() {
            drop(CString::from_raw(info.name));
        }
    }
}

#[no_mangle]
pub unsafe extern "C" fn ffi_free_member_info(info: *mut FfiMemberInfo) {
    if !info.is_null() {
        let info = Box::from_raw(info);
        if !info.path.is_null() {
            drop(CString::from_raw(info.path));
        }
        if !info.suffix.is_null() {
            drop(CString::from_raw(info.suffix));
        }
    }
}

#[no_mangle]
pub unsafe extern "C" fn ffi_free_member_info_array(arr: *mut FfiMemberInfo, count: usize) {
    if !arr.is_null() {
        let slice = Box::from_raw(std::slice::from_raw_parts_mut(arr, count));
        for info in slice.iter() {
            if !info.path.is_null() {
                drop(CString::from_raw(info.path));
            }
            if !info.suffix.is_null() {
                drop(CString::from_raw(info.suffix));
            }
        }
    }
}

#[no_mangle]
pub unsafe extern "C" fn ffi_free_column_info(info: *mut FfiColumnInfo) {
    if !info.is_null() {
        let info = Box::from_raw(info);
        if !info.name.is_null() {
            drop(CString::from_raw(info.name));
        }
    }
}

#[no_mangle]
pub unsafe extern "C" fn ffi_free_cell(cell: *mut FfiResolvedCell) {
    if !cell.is_null() {
        let cell = Box::from_raw(cell);
        if !cell.value.string_value.is_null() {
            drop(CString::from_raw(cell.value.string_value));
        }
        if !cell.source_path.is_null() {
            drop(CString::from_raw(cell.source_path));
        }
    }
}

#[no_mangle]
pub unsafe extern "C" fn ffi_free_index_array(arr: *mut usize) {
    if !arr.is_null() {
        // We need to know the length, but we don't have it here
        // This is a limitation - caller must track length
        // For now, just leak it (not ideal, but safe)
        // TODO: Return a struct with length
    }
}

#[no_mangle]
pub unsafe extern "C" fn ffi_free_history_entry(entry: *mut FfiHistoryEntry) {
    if !entry.is_null() {
        let entry = Box::from_raw(entry);
        if !entry.family.is_null() {
            drop(CString::from_raw(entry.family));
        }
        if !entry.timestamp.is_null() {
            drop(CString::from_raw(entry.timestamp));
        }
        if !entry.patch_file.is_null() {
            drop(CString::from_raw(entry.patch_file));
        }
    }
}

#[no_mangle]
pub unsafe extern "C" fn ffi_free_history_entry_array(arr: *mut *mut FfiHistoryEntry, count: usize) {
    if !arr.is_null() {
        let slice = std::slice::from_raw_parts_mut(arr, count);
        for entry_ptr in slice.iter() {
            if !entry_ptr.is_null() {
                ffi_free_history_entry(*entry_ptr);
            }
        }
        drop(Box::from_raw(
            std::slice::from_raw_parts_mut(arr, count) as *mut [*mut FfiHistoryEntry]
        ));
    }
}
