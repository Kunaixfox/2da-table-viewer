#ifndef DA_FFI_H
#define DA_FFI_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Opaque handle types
typedef struct FfiScanResult FfiScanResult;
typedef struct FfiResolvedTable FfiResolvedTable;
typedef struct FfiPatchResult FfiPatchResult;
typedef struct FfiHistoryFile FfiHistoryFile;

// String result for error messages and other strings
typedef struct {
    char* data;
    size_t len;
    int success;  // 1 = success, 0 = error (data contains error message)
} FfiStringResult;

// Family info returned from scan
typedef struct {
    char* name;
    size_t member_count;
} FfiFamilyInfo;

// Family member info
typedef struct {
    char* path;
    char* suffix;  // NULL for base file
    int is_base;
} FfiMemberInfo;

// Cell value with type tag
typedef struct {
    int value_type;  // 0=Empty, 1=Integer, 2=Float, 3=String
    int64_t int_value;
    double float_value;
    char* string_value;
} FfiCellValue;

// Resolved cell with provenance
typedef struct {
    FfiCellValue value;
    char* source_path;
} FfiResolvedCell;

// Column info
typedef struct {
    char* name;
    size_t index;
} FfiColumnInfo;

// Edit for patches
typedef struct {
    int64_t row_id;
    char* column;
    char* value;
} FfiEdit;

// History entry
typedef struct {
    char* family;
    char* timestamp;
    size_t edit_count;
    char* patch_file;
} FfiHistoryEntry;

// ============================================================================
// Scanning and Family Operations
// ============================================================================

// Scan a directory for CSV files and group into families
// Returns NULL on error (check ffi_last_error)
FfiScanResult* ffi_scan_directory(const char* root_path);

// Get number of families in scan result
size_t ffi_scan_family_count(const FfiScanResult* result);

// Get family info by index (0-based)
// Returns family info (caller must free with ffi_free_family_info)
FfiFamilyInfo* ffi_scan_get_family(const FfiScanResult* result, size_t index);

// Get members of a family by family name
// Returns array of member info, sets out_count
FfiMemberInfo* ffi_scan_get_members(const FfiScanResult* result,
                                     const char* family_name,
                                     size_t* out_count);

// Search families by name pattern (case-insensitive substring)
// Returns array of matching family names, sets out_count
char** ffi_search_families(const FfiScanResult* result,
                           const char* pattern,
                           size_t* out_count);

// Free scan result
void ffi_scan_free(FfiScanResult* result);

// ============================================================================
// Table Operations
// ============================================================================

// Merge a family into a resolved table
// Returns NULL on error (check ffi_last_error)
FfiResolvedTable* ffi_merge_family(const FfiScanResult* scan_result,
                                    const char* family_name);

// Get column count
size_t ffi_table_column_count(const FfiResolvedTable* table);

// Get row count
size_t ffi_table_row_count(const FfiResolvedTable* table);

// Get column info by index
FfiColumnInfo* ffi_table_get_column(const FfiResolvedTable* table, size_t index);

// Get cell at row/column
// row_index is 0-based index in the table
FfiResolvedCell* ffi_table_get_cell(const FfiResolvedTable* table,
                                     size_t row_index,
                                     size_t col_index);

// Get row ID for a given row index
int64_t ffi_table_get_row_id(const FfiResolvedTable* table, size_t row_index);

// Filter rows by column value (case-insensitive substring)
// Returns array of matching row indices, sets out_count
size_t* ffi_table_filter_rows(const FfiResolvedTable* table,
                               const char* column_name,
                               const char* value_pattern,
                               size_t* out_count);

// Free resolved table
void ffi_table_free(FfiResolvedTable* table);

// ============================================================================
// Patch Operations
// ============================================================================

// Create a new patch (returns JSON string)
FfiStringResult ffi_create_patch(const char* family_name);

// Apply a patch and export modified files
// patch_json: JSON string of the patch
// output_dir: Directory to write modified files
// history_path: Path to history.json (NULL to skip history)
FfiPatchResult* ffi_apply_patch(const FfiScanResult* scan_result,
                                 const char* patch_json,
                                 const char* output_dir,
                                 const char* history_path);

// Validate a patch without applying
// Returns NULL if valid, error message if invalid
FfiStringResult ffi_validate_patch(const FfiScanResult* scan_result,
                                    const char* patch_json);

// Get number of files exported from patch result
size_t ffi_patch_export_count(const FfiPatchResult* result);

// Get exported file path by index
char* ffi_patch_get_export_path(const FfiPatchResult* result, size_t index);

// Free patch result
void ffi_patch_free(FfiPatchResult* result);

// ============================================================================
// History Operations
// ============================================================================

// Load history file
FfiHistoryFile* ffi_history_load(const char* path);

// Get history entry count
size_t ffi_history_count(const FfiHistoryFile* history);

// Get history entry by index (most recent first)
FfiHistoryEntry* ffi_history_get_entry(const FfiHistoryFile* history, size_t index);

// Get entries for a specific family
FfiHistoryEntry** ffi_history_get_family_entries(const FfiHistoryFile* history,
                                                  const char* family_name,
                                                  size_t* out_count);

// Free history file
void ffi_history_free(FfiHistoryFile* history);

// ============================================================================
// Memory Management
// ============================================================================

void ffi_free_string(char* s);
void ffi_free_string_array(char** arr, size_t count);
void ffi_free_family_info(FfiFamilyInfo* info);
void ffi_free_member_info(FfiMemberInfo* info);
void ffi_free_member_info_array(FfiMemberInfo* arr, size_t count);
void ffi_free_column_info(FfiColumnInfo* info);
void ffi_free_cell(FfiResolvedCell* cell);
void ffi_free_index_array(size_t* arr);
void ffi_free_history_entry(FfiHistoryEntry* entry);
void ffi_free_history_entry_array(FfiHistoryEntry** arr, size_t count);

// ============================================================================
// Error Handling
// ============================================================================

// Get last error message (thread-local)
// Returns NULL if no error
const char* ffi_last_error(void);

// Clear last error
void ffi_clear_error(void);

#ifdef __cplusplus
}
#endif

#endif // DA_FFI_H
