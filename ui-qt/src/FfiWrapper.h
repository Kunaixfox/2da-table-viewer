#ifndef FFIWRAPPER_H
#define FFIWRAPPER_H

#include <QString>
#include <QLibrary>
#include <da_ffi.h>

class FfiWrapper
{
public:
    static FfiWrapper& instance();

    bool initialize();
    bool isInitialized() const { return m_initialized; }

    QString lastError();
    void clearError();

    // Scanning
    FfiScanResult* scanDirectory(const QString& path);
    size_t scanFamilyCount(const FfiScanResult* result);
    FfiFamilyInfo* scanGetFamily(const FfiScanResult* result, size_t index);
    FfiMemberInfo* scanGetMembers(const FfiScanResult* result,
                                   const QString& familyName,
                                   size_t* outCount);
    QStringList searchFamilies(const FfiScanResult* result, const QString& pattern);
    void scanFree(FfiScanResult* result);

    // Tables
    FfiResolvedTable* mergeFamily(const FfiScanResult* scanResult,
                                   const QString& familyName);
    size_t tableColumnCount(const FfiResolvedTable* table);
    size_t tableRowCount(const FfiResolvedTable* table);
    FfiColumnInfo* tableGetColumn(const FfiResolvedTable* table, size_t index);
    FfiResolvedCell* tableGetCell(const FfiResolvedTable* table,
                                   size_t rowIndex, size_t colIndex);
    int64_t tableGetRowId(const FfiResolvedTable* table, size_t rowIndex);
    QList<size_t> tableFilterRows(const FfiResolvedTable* table,
                                   const QString& columnName,
                                   const QString& valuePattern);
    void tableFree(FfiResolvedTable* table);

    // Patches
    QString createPatch(const QString& familyName);
    FfiPatchResult* applyPatch(const FfiScanResult* scanResult,
                                const QString& patchJson,
                                const QString& outputDir,
                                const QString& historyPath);
    QString validatePatch(const FfiScanResult* scanResult, const QString& patchJson);
    size_t patchExportCount(const FfiPatchResult* result);
    QString patchGetExportPath(const FfiPatchResult* result, size_t index);
    void patchFree(FfiPatchResult* result);

    // History
    FfiHistoryFile* historyLoad(const QString& path);
    size_t historyCount(const FfiHistoryFile* history);
    FfiHistoryEntry* historyGetEntry(const FfiHistoryFile* history, size_t index);
    void historyFree(FfiHistoryFile* history);

    // Memory
    void freeString(char* s);
    void freeFamilyInfo(FfiFamilyInfo* info);
    void freeMemberInfo(FfiMemberInfo* info);
    void freeMemberInfoArray(FfiMemberInfo* arr, size_t count);
    void freeColumnInfo(FfiColumnInfo* info);
    void freeCell(FfiResolvedCell* cell);
    void freeHistoryEntry(FfiHistoryEntry* entry);

private:
    FfiWrapper();
    ~FfiWrapper();
    FfiWrapper(const FfiWrapper&) = delete;
    FfiWrapper& operator=(const FfiWrapper&) = delete;

    bool loadFunction(const char* name, void** funcPtr);

    QLibrary m_library;
    bool m_initialized;

    // Function pointers
    decltype(&ffi_scan_directory) m_ffi_scan_directory;
    decltype(&ffi_scan_family_count) m_ffi_scan_family_count;
    decltype(&ffi_scan_get_family) m_ffi_scan_get_family;
    decltype(&ffi_scan_get_members) m_ffi_scan_get_members;
    decltype(&ffi_search_families) m_ffi_search_families;
    decltype(&ffi_scan_free) m_ffi_scan_free;

    decltype(&ffi_merge_family) m_ffi_merge_family;
    decltype(&ffi_table_column_count) m_ffi_table_column_count;
    decltype(&ffi_table_row_count) m_ffi_table_row_count;
    decltype(&ffi_table_get_column) m_ffi_table_get_column;
    decltype(&ffi_table_get_cell) m_ffi_table_get_cell;
    decltype(&ffi_table_get_row_id) m_ffi_table_get_row_id;
    decltype(&ffi_table_filter_rows) m_ffi_table_filter_rows;
    decltype(&ffi_table_free) m_ffi_table_free;

    decltype(&ffi_create_patch) m_ffi_create_patch;
    decltype(&ffi_apply_patch) m_ffi_apply_patch;
    decltype(&ffi_validate_patch) m_ffi_validate_patch;
    decltype(&ffi_patch_export_count) m_ffi_patch_export_count;
    decltype(&ffi_patch_get_export_path) m_ffi_patch_get_export_path;
    decltype(&ffi_patch_free) m_ffi_patch_free;

    decltype(&ffi_history_load) m_ffi_history_load;
    decltype(&ffi_history_count) m_ffi_history_count;
    decltype(&ffi_history_get_entry) m_ffi_history_get_entry;
    decltype(&ffi_history_free) m_ffi_history_free;

    decltype(&ffi_free_string) m_ffi_free_string;
    decltype(&ffi_free_string_array) m_ffi_free_string_array;
    decltype(&ffi_free_family_info) m_ffi_free_family_info;
    decltype(&ffi_free_member_info) m_ffi_free_member_info;
    decltype(&ffi_free_member_info_array) m_ffi_free_member_info_array;
    decltype(&ffi_free_column_info) m_ffi_free_column_info;
    decltype(&ffi_free_cell) m_ffi_free_cell;
    decltype(&ffi_free_index_array) m_ffi_free_index_array;
    decltype(&ffi_free_history_entry) m_ffi_free_history_entry;

    decltype(&ffi_last_error) m_ffi_last_error;
    decltype(&ffi_clear_error) m_ffi_clear_error;
};

#endif // FFIWRAPPER_H
