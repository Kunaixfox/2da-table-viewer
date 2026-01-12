#include "FfiWrapper.h"
#include <QCoreApplication>
#include <QDir>
#include <QDebug>

FfiWrapper& FfiWrapper::instance()
{
    static FfiWrapper instance;
    return instance;
}

FfiWrapper::FfiWrapper()
    : m_initialized(false)
    , m_ffi_scan_directory(nullptr)
    , m_ffi_scan_family_count(nullptr)
    , m_ffi_scan_get_family(nullptr)
    , m_ffi_scan_get_members(nullptr)
    , m_ffi_search_families(nullptr)
    , m_ffi_scan_free(nullptr)
    , m_ffi_merge_family(nullptr)
    , m_ffi_table_column_count(nullptr)
    , m_ffi_table_row_count(nullptr)
    , m_ffi_table_get_column(nullptr)
    , m_ffi_table_get_cell(nullptr)
    , m_ffi_table_get_row_id(nullptr)
    , m_ffi_table_filter_rows(nullptr)
    , m_ffi_table_free(nullptr)
    , m_ffi_create_patch(nullptr)
    , m_ffi_apply_patch(nullptr)
    , m_ffi_validate_patch(nullptr)
    , m_ffi_patch_export_count(nullptr)
    , m_ffi_patch_get_export_path(nullptr)
    , m_ffi_patch_free(nullptr)
    , m_ffi_history_load(nullptr)
    , m_ffi_history_count(nullptr)
    , m_ffi_history_get_entry(nullptr)
    , m_ffi_history_free(nullptr)
    , m_ffi_free_string(nullptr)
    , m_ffi_free_string_array(nullptr)
    , m_ffi_free_family_info(nullptr)
    , m_ffi_free_member_info(nullptr)
    , m_ffi_free_member_info_array(nullptr)
    , m_ffi_free_column_info(nullptr)
    , m_ffi_free_cell(nullptr)
    , m_ffi_free_index_array(nullptr)
    , m_ffi_free_history_entry(nullptr)
    , m_ffi_last_error(nullptr)
    , m_ffi_clear_error(nullptr)
{
}

FfiWrapper::~FfiWrapper()
{
    if (m_library.isLoaded()) {
        m_library.unload();
    }
}

bool FfiWrapper::loadFunction(const char* name, void** funcPtr)
{
    *funcPtr = (void*)m_library.resolve(name);
    if (!*funcPtr) {
        qWarning() << "Failed to resolve FFI function:" << name;
        return false;
    }
    return true;
}

bool FfiWrapper::initialize()
{
    if (m_initialized) {
        return true;
    }

    // Try to load from application directory first
    QString dllPath = QCoreApplication::applicationDirPath() + "/da_ffi.dll";
    m_library.setFileName(dllPath);

    if (!m_library.load()) {
        // Try just the name (relies on PATH)
        m_library.setFileName("da_ffi");
        if (!m_library.load()) {
            qWarning() << "Failed to load da_ffi.dll:" << m_library.errorString();
            return false;
        }
    }

    // Load all function pointers
    bool success = true;

    success &= loadFunction("ffi_scan_directory", (void**)&m_ffi_scan_directory);
    success &= loadFunction("ffi_scan_family_count", (void**)&m_ffi_scan_family_count);
    success &= loadFunction("ffi_scan_get_family", (void**)&m_ffi_scan_get_family);
    success &= loadFunction("ffi_scan_get_members", (void**)&m_ffi_scan_get_members);
    success &= loadFunction("ffi_search_families", (void**)&m_ffi_search_families);
    success &= loadFunction("ffi_scan_free", (void**)&m_ffi_scan_free);

    success &= loadFunction("ffi_merge_family", (void**)&m_ffi_merge_family);
    success &= loadFunction("ffi_table_column_count", (void**)&m_ffi_table_column_count);
    success &= loadFunction("ffi_table_row_count", (void**)&m_ffi_table_row_count);
    success &= loadFunction("ffi_table_get_column", (void**)&m_ffi_table_get_column);
    success &= loadFunction("ffi_table_get_cell", (void**)&m_ffi_table_get_cell);
    success &= loadFunction("ffi_table_get_row_id", (void**)&m_ffi_table_get_row_id);
    success &= loadFunction("ffi_table_filter_rows", (void**)&m_ffi_table_filter_rows);
    success &= loadFunction("ffi_table_free", (void**)&m_ffi_table_free);

    success &= loadFunction("ffi_create_patch", (void**)&m_ffi_create_patch);
    success &= loadFunction("ffi_apply_patch", (void**)&m_ffi_apply_patch);
    success &= loadFunction("ffi_validate_patch", (void**)&m_ffi_validate_patch);
    success &= loadFunction("ffi_patch_export_count", (void**)&m_ffi_patch_export_count);
    success &= loadFunction("ffi_patch_get_export_path", (void**)&m_ffi_patch_get_export_path);
    success &= loadFunction("ffi_patch_free", (void**)&m_ffi_patch_free);

    success &= loadFunction("ffi_history_load", (void**)&m_ffi_history_load);
    success &= loadFunction("ffi_history_count", (void**)&m_ffi_history_count);
    success &= loadFunction("ffi_history_get_entry", (void**)&m_ffi_history_get_entry);
    success &= loadFunction("ffi_history_free", (void**)&m_ffi_history_free);

    success &= loadFunction("ffi_free_string", (void**)&m_ffi_free_string);
    success &= loadFunction("ffi_free_string_array", (void**)&m_ffi_free_string_array);
    success &= loadFunction("ffi_free_family_info", (void**)&m_ffi_free_family_info);
    success &= loadFunction("ffi_free_member_info", (void**)&m_ffi_free_member_info);
    success &= loadFunction("ffi_free_member_info_array", (void**)&m_ffi_free_member_info_array);
    success &= loadFunction("ffi_free_column_info", (void**)&m_ffi_free_column_info);
    success &= loadFunction("ffi_free_cell", (void**)&m_ffi_free_cell);
    success &= loadFunction("ffi_free_index_array", (void**)&m_ffi_free_index_array);
    success &= loadFunction("ffi_free_history_entry", (void**)&m_ffi_free_history_entry);

    success &= loadFunction("ffi_last_error", (void**)&m_ffi_last_error);
    success &= loadFunction("ffi_clear_error", (void**)&m_ffi_clear_error);

    if (!success) {
        m_library.unload();
        return false;
    }

    m_initialized = true;
    return true;
}

QString FfiWrapper::lastError()
{
    if (!m_ffi_last_error) return QString();
    const char* err = m_ffi_last_error();
    return err ? QString::fromUtf8(err) : QString();
}

void FfiWrapper::clearError()
{
    if (m_ffi_clear_error) {
        m_ffi_clear_error();
    }
}

// Scanning
FfiScanResult* FfiWrapper::scanDirectory(const QString& path)
{
    if (!m_ffi_scan_directory) return nullptr;
    QByteArray pathBytes = path.toUtf8();
    return m_ffi_scan_directory(pathBytes.constData());
}

size_t FfiWrapper::scanFamilyCount(const FfiScanResult* result)
{
    if (!m_ffi_scan_family_count || !result) return 0;
    return m_ffi_scan_family_count(result);
}

FfiFamilyInfo* FfiWrapper::scanGetFamily(const FfiScanResult* result, size_t index)
{
    if (!m_ffi_scan_get_family || !result) return nullptr;
    return m_ffi_scan_get_family(result, index);
}

FfiMemberInfo* FfiWrapper::scanGetMembers(const FfiScanResult* result,
                                           const QString& familyName,
                                           size_t* outCount)
{
    if (!m_ffi_scan_get_members || !result) return nullptr;
    QByteArray nameBytes = familyName.toUtf8();
    return m_ffi_scan_get_members(result, nameBytes.constData(), outCount);
}

QStringList FfiWrapper::searchFamilies(const FfiScanResult* result, const QString& pattern)
{
    QStringList results;
    if (!m_ffi_search_families || !result) return results;

    QByteArray patternBytes = pattern.toUtf8();
    size_t count = 0;
    char** names = m_ffi_search_families(result, patternBytes.constData(), &count);

    if (names) {
        for (size_t i = 0; i < count; ++i) {
            if (names[i]) {
                results.append(QString::fromUtf8(names[i]));
            }
        }
        m_ffi_free_string_array(names, count);
    }

    return results;
}

void FfiWrapper::scanFree(FfiScanResult* result)
{
    if (m_ffi_scan_free && result) {
        m_ffi_scan_free(result);
    }
}

// Tables
FfiResolvedTable* FfiWrapper::mergeFamily(const FfiScanResult* scanResult,
                                           const QString& familyName)
{
    if (!m_ffi_merge_family || !scanResult) return nullptr;
    QByteArray nameBytes = familyName.toUtf8();
    return m_ffi_merge_family(scanResult, nameBytes.constData());
}

size_t FfiWrapper::tableColumnCount(const FfiResolvedTable* table)
{
    if (!m_ffi_table_column_count || !table) return 0;
    return m_ffi_table_column_count(table);
}

size_t FfiWrapper::tableRowCount(const FfiResolvedTable* table)
{
    if (!m_ffi_table_row_count || !table) return 0;
    return m_ffi_table_row_count(table);
}

FfiColumnInfo* FfiWrapper::tableGetColumn(const FfiResolvedTable* table, size_t index)
{
    if (!m_ffi_table_get_column || !table) return nullptr;
    return m_ffi_table_get_column(table, index);
}

FfiResolvedCell* FfiWrapper::tableGetCell(const FfiResolvedTable* table,
                                           size_t rowIndex, size_t colIndex)
{
    if (!m_ffi_table_get_cell || !table) return nullptr;
    return m_ffi_table_get_cell(table, rowIndex, colIndex);
}

int64_t FfiWrapper::tableGetRowId(const FfiResolvedTable* table, size_t rowIndex)
{
    if (!m_ffi_table_get_row_id || !table) return -1;
    return m_ffi_table_get_row_id(table, rowIndex);
}

QList<size_t> FfiWrapper::tableFilterRows(const FfiResolvedTable* table,
                                           const QString& columnName,
                                           const QString& valuePattern)
{
    QList<size_t> results;
    if (!m_ffi_table_filter_rows || !table) return results;

    QByteArray colBytes = columnName.toUtf8();
    QByteArray valBytes = valuePattern.toUtf8();
    size_t count = 0;
    size_t* indices = m_ffi_table_filter_rows(table, colBytes.constData(),
                                               valBytes.constData(), &count);

    if (indices) {
        for (size_t i = 0; i < count; ++i) {
            results.append(indices[i]);
        }
        m_ffi_free_index_array(indices);
    }

    return results;
}

void FfiWrapper::tableFree(FfiResolvedTable* table)
{
    if (m_ffi_table_free && table) {
        m_ffi_table_free(table);
    }
}

// Patches
QString FfiWrapper::createPatch(const QString& familyName)
{
    if (!m_ffi_create_patch) return QString();
    QByteArray nameBytes = familyName.toUtf8();
    FfiStringResult result = m_ffi_create_patch(nameBytes.constData());

    QString str;
    if (result.data) {
        str = QString::fromUtf8(result.data, result.len);
        m_ffi_free_string(result.data);
    }
    return str;
}

FfiPatchResult* FfiWrapper::applyPatch(const FfiScanResult* scanResult,
                                        const QString& patchJson,
                                        const QString& outputDir,
                                        const QString& historyPath)
{
    if (!m_ffi_apply_patch || !scanResult) return nullptr;
    QByteArray jsonBytes = patchJson.toUtf8();
    QByteArray outBytes = outputDir.toUtf8();
    QByteArray histBytes = historyPath.isEmpty() ? QByteArray() : historyPath.toUtf8();
    return m_ffi_apply_patch(scanResult, jsonBytes.constData(), outBytes.constData(),
                             histBytes.isEmpty() ? nullptr : histBytes.constData());
}

QString FfiWrapper::validatePatch(const FfiScanResult* scanResult, const QString& patchJson)
{
    if (!m_ffi_validate_patch || !scanResult) return QString();
    QByteArray jsonBytes = patchJson.toUtf8();
    FfiStringResult result = m_ffi_validate_patch(scanResult, jsonBytes.constData());

    QString str;
    if (result.data) {
        str = QString::fromUtf8(result.data, result.len);
        m_ffi_free_string(result.data);
    }
    return result.success ? QString() : str;
}

size_t FfiWrapper::patchExportCount(const FfiPatchResult* result)
{
    if (!m_ffi_patch_export_count || !result) return 0;
    return m_ffi_patch_export_count(result);
}

QString FfiWrapper::patchGetExportPath(const FfiPatchResult* result, size_t index)
{
    if (!m_ffi_patch_get_export_path || !result) return QString();
    char* path = m_ffi_patch_get_export_path(result, index);
    QString str;
    if (path) {
        str = QString::fromUtf8(path);
        m_ffi_free_string(path);
    }
    return str;
}

void FfiWrapper::patchFree(FfiPatchResult* result)
{
    if (m_ffi_patch_free && result) {
        m_ffi_patch_free(result);
    }
}

// History
FfiHistoryFile* FfiWrapper::historyLoad(const QString& path)
{
    if (!m_ffi_history_load) return nullptr;
    QByteArray pathBytes = path.toUtf8();
    return m_ffi_history_load(pathBytes.constData());
}

size_t FfiWrapper::historyCount(const FfiHistoryFile* history)
{
    if (!m_ffi_history_count || !history) return 0;
    return m_ffi_history_count(history);
}

FfiHistoryEntry* FfiWrapper::historyGetEntry(const FfiHistoryFile* history, size_t index)
{
    if (!m_ffi_history_get_entry || !history) return nullptr;
    return m_ffi_history_get_entry(history, index);
}

void FfiWrapper::historyFree(FfiHistoryFile* history)
{
    if (m_ffi_history_free && history) {
        m_ffi_history_free(history);
    }
}

// Memory
void FfiWrapper::freeString(char* s)
{
    if (m_ffi_free_string && s) {
        m_ffi_free_string(s);
    }
}

void FfiWrapper::freeFamilyInfo(FfiFamilyInfo* info)
{
    if (m_ffi_free_family_info && info) {
        m_ffi_free_family_info(info);
    }
}

void FfiWrapper::freeMemberInfo(FfiMemberInfo* info)
{
    if (m_ffi_free_member_info && info) {
        m_ffi_free_member_info(info);
    }
}

void FfiWrapper::freeMemberInfoArray(FfiMemberInfo* arr, size_t count)
{
    if (m_ffi_free_member_info_array && arr) {
        m_ffi_free_member_info_array(arr, count);
    }
}

void FfiWrapper::freeColumnInfo(FfiColumnInfo* info)
{
    if (m_ffi_free_column_info && info) {
        m_ffi_free_column_info(info);
    }
}

void FfiWrapper::freeCell(FfiResolvedCell* cell)
{
    if (m_ffi_free_cell && cell) {
        m_ffi_free_cell(cell);
    }
}

void FfiWrapper::freeHistoryEntry(FfiHistoryEntry* entry)
{
    if (m_ffi_free_history_entry && entry) {
        m_ffi_free_history_entry(entry);
    }
}
