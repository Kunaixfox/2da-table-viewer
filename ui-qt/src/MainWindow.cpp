#include "MainWindow.h"
#include "FamilyPanel.h"
#include "TablePanel.h"
#include "DetailsPanel.h"
#include "FfiWrapper.h"

#include <QMenuBar>
#include <QMenu>
#include <QToolBar>
#include <QStatusBar>
#include <QFileDialog>
#include <QMessageBox>
#include <QLabel>
#include <QSettings>
#include <QFile>
#include <QTextStream>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDir>

// Helper function to escape CSV values
static QString escapeCsv(const QString& s)
{
    if (s.contains(',') || s.contains('"') || s.contains('\n')) {
        return QString("\"%1\"").arg(QString(s).replace("\"", "\"\""));
    }
    return s;
}

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_splitter(nullptr)
    , m_familyPanel(nullptr)
    , m_tablePanel(nullptr)
    , m_detailsPanel(nullptr)
    , m_scanResult(nullptr)
{
    setWindowTitle("DA Table Viewer");
    resize(1200, 800);

    createMenus();
    createToolBar();
    createStatusBar();
    createPanels();

    // Restore last opened folder
    QSettings settings;
    QString lastPath = settings.value("lastRootPath").toString();
    if (!lastPath.isEmpty() && QDir(lastPath).exists()) {
        loadFolder(lastPath);
    }
}

MainWindow::~MainWindow()
{
    if (m_scanResult) {
        FfiWrapper::instance().scanFree(m_scanResult);
    }
}

void MainWindow::createMenus()
{
    // File menu
    QMenu* fileMenu = menuBar()->addMenu(tr("&File"));

    QAction* openAction = fileMenu->addAction(tr("&Open Folder..."));
    openAction->setShortcut(QKeySequence::Open);
    connect(openAction, &QAction::triggered, this, &MainWindow::onOpenFolder);

    fileMenu->addSeparator();

    QAction* exportAction = fileMenu->addAction(tr("&Export..."));
    exportAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_E));
    connect(exportAction, &QAction::triggered, this, &MainWindow::onExport);

    fileMenu->addSeparator();

    QAction* exitAction = fileMenu->addAction(tr("E&xit"));
    exitAction->setShortcut(QKeySequence::Quit);
    connect(exitAction, &QAction::triggered, this, &QMainWindow::close);

    // Edit menu
    QMenu* editMenu = menuBar()->addMenu(tr("&Edit"));

    QAction* undoAction = editMenu->addAction(tr("&Undo"));
    undoAction->setShortcut(QKeySequence::Undo);
    connect(undoAction, &QAction::triggered, this, &MainWindow::onUndo);

    QAction* redoAction = editMenu->addAction(tr("&Redo"));
    redoAction->setShortcut(QKeySequence::Redo);
    connect(redoAction, &QAction::triggered, this, &MainWindow::onRedo);

    // Patch menu
    QMenu* patchMenu = menuBar()->addMenu(tr("&Patch"));

    QAction* savePatchAction = patchMenu->addAction(tr("&Save Patch..."));
    savePatchAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_S));
    connect(savePatchAction, &QAction::triggered, this, &MainWindow::onSavePatch);

    QAction* importPatchAction = patchMenu->addAction(tr("&Import Patch..."));
    importPatchAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_I));
    connect(importPatchAction, &QAction::triggered, this, &MainWindow::onImportPatch);

    patchMenu->addSeparator();

    QAction* applyPatchAction = patchMenu->addAction(tr("&Apply Patch..."));
    applyPatchAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_A));
    connect(applyPatchAction, &QAction::triggered, this, &MainWindow::onApplyPatch);

    // Help menu
    QMenu* helpMenu = menuBar()->addMenu(tr("&Help"));

    QAction* aboutAction = helpMenu->addAction(tr("&About"));
    connect(aboutAction, &QAction::triggered, this, &MainWindow::onAbout);
}

void MainWindow::createToolBar()
{
    QToolBar* toolbar = addToolBar(tr("Main"));
    toolbar->setMovable(false);

    QAction* openAction = toolbar->addAction(tr("Open Folder"));
    connect(openAction, &QAction::triggered, this, &MainWindow::onOpenFolder);

    QAction* exportAction = toolbar->addAction(tr("Export"));
    connect(exportAction, &QAction::triggered, this, &MainWindow::onExport);

    toolbar->addSeparator();

    QAction* undoAction = toolbar->addAction(tr("Undo"));
    connect(undoAction, &QAction::triggered, this, &MainWindow::onUndo);

    QAction* redoAction = toolbar->addAction(tr("Redo"));
    connect(redoAction, &QAction::triggered, this, &MainWindow::onRedo);

    toolbar->addSeparator();

    QAction* savePatchAction = toolbar->addAction(tr("Save Patch"));
    connect(savePatchAction, &QAction::triggered, this, &MainWindow::onSavePatch);

    QAction* importPatchAction = toolbar->addAction(tr("Import Patch"));
    connect(importPatchAction, &QAction::triggered, this, &MainWindow::onImportPatch);

    QAction* applyPatchAction = toolbar->addAction(tr("Apply Patch"));
    connect(applyPatchAction, &QAction::triggered, this, &MainWindow::onApplyPatch);
}

void MainWindow::createStatusBar()
{
    statusBar()->showMessage(tr("Ready"));
}

void MainWindow::createPanels()
{
    m_splitter = new QSplitter(Qt::Horizontal, this);

    // Left panel - Family browser
    m_familyPanel = new FamilyPanel(this);
    m_familyPanel->setMinimumWidth(180);
    m_familyPanel->setMaximumWidth(300);

    // Center panel - Table view
    m_tablePanel = new TablePanel(this);

    // Right panel - Details (tabbed)
    m_detailsPanel = new DetailsPanel(this);
    m_detailsPanel->setMinimumWidth(200);
    m_detailsPanel->setMaximumWidth(350);

    m_splitter->addWidget(m_familyPanel);
    m_splitter->addWidget(m_tablePanel);
    m_splitter->addWidget(m_detailsPanel);

    // Set initial sizes (left: 200, center: stretch, right: 250)
    m_splitter->setSizes({200, 600, 250});

    setCentralWidget(m_splitter);

    // Connect signals
    connect(m_familyPanel, &FamilyPanel::familySelected,
            this, &MainWindow::onFamilySelected);

    connect(m_tablePanel, &TablePanel::cellSelected,
            this, &MainWindow::onCellSelected);

    connect(m_tablePanel, &TablePanel::cellEdited,
            this, [this](int row, int col, const QString& newValue) {
        // Convert row index and column index to row ID and column name
        FfiResolvedTable* table = m_tablePanel->resolvedTable();
        if (!table) return;

        FfiWrapper& ffi = FfiWrapper::instance();

        // Get row ID
        int64_t rowId = ffi.tableGetRowId(table, row);

        // Get column name
        FfiColumnInfo* colInfo = ffi.tableGetColumn(table, col);
        if (!colInfo || !colInfo->name) {
            if (colInfo) ffi.freeColumnInfo(colInfo);
            return;
        }
        QString columnName = QString::fromUtf8(colInfo->name);
        ffi.freeColumnInfo(colInfo);

        // Call the edit handler
        onEditRequested(rowId, columnName, newValue);
    });

    connect(m_detailsPanel, &DetailsPanel::editRequested,
            this, &MainWindow::onEditRequested);

    connect(m_detailsPanel, &DetailsPanel::clearEditsRequested, this, [this]() {
        m_pendingEdits.clear();
        m_undoStack.clear();
        updateWindowTitle();
        statusBar()->showMessage(tr("All pending edits cleared"));
    });

    connect(m_detailsPanel, &DetailsPanel::undoHistoryRequested,
            this, [this](const QString& family, const QString& outputDir) {
        // Restore original source files for this family
        if (!m_scanResult || family.isEmpty() || outputDir.isEmpty()) {
            return;
        }

        FfiWrapper& ffi = FfiWrapper::instance();

        // Get family members to find source files
        size_t memberCount = 0;
        FfiMemberInfo* members = ffi.scanGetMembers(m_scanResult, family, &memberCount);

        if (!members || memberCount == 0) {
            QMessageBox::warning(this, tr("Undo Failed"),
                tr("Could not find source files for family '%1'").arg(family));
            return;
        }

        int filesRestored = 0;
        for (size_t i = 0; i < memberCount; ++i) {
            if (members[i].path) {
                QString srcPath = QString::fromUtf8(members[i].path);
                QString fileName = QFileInfo(srcPath).fileName();
                QString destPath = QDir(outputDir).filePath(fileName);

                if (QFile::copy(srcPath, destPath)) {
                    ++filesRestored;
                }
            }
        }

        ffi.freeMemberInfoArray(members, memberCount);

        QMessageBox::information(this, tr("Undo Complete"),
            tr("Restored %1 original file(s) to:\n%2").arg(filesRestored).arg(outputDir));

        m_detailsPanel->refreshHistory();
        statusBar()->showMessage(tr("Restored %1 files").arg(filesRestored));
    });
}

void MainWindow::loadFolder(const QString& path)
{
    // Free previous scan result
    if (m_scanResult) {
        FfiWrapper::instance().scanFree(m_scanResult);
        m_scanResult = nullptr;
    }

    m_rootPath = path;

    // Scan directory
    m_scanResult = FfiWrapper::instance().scanDirectory(path);

    if (!m_scanResult) {
        QString error = FfiWrapper::instance().lastError();
        QMessageBox::warning(this, tr("Scan Error"),
            tr("Failed to scan directory:\n%1").arg(error));
        return;
    }

    // Save path for next launch
    QSettings settings;
    settings.setValue("lastRootPath", path);

    // Update family panel
    m_familyPanel->loadFamilies(m_scanResult);

    // Clear table and details
    m_tablePanel->clear();
    m_detailsPanel->clear();
    m_currentFamily.clear();
    m_pendingEdits.clear();

    updateWindowTitle();

    size_t familyCount = FfiWrapper::instance().scanFamilyCount(m_scanResult);
    statusBar()->showMessage(tr("Loaded %1 families from %2")
        .arg(familyCount).arg(path));
}

void MainWindow::updateWindowTitle()
{
    QString title = "DA Table Viewer";
    if (!m_rootPath.isEmpty()) {
        title += " - " + m_rootPath;
    }
    if (!m_currentFamily.isEmpty()) {
        title += " [" + m_currentFamily + "]";
    }
    if (!m_pendingEdits.isEmpty()) {
        title += " *";
    }
    setWindowTitle(title);
}

void MainWindow::onOpenFolder()
{
    QString path = QFileDialog::getExistingDirectory(this,
        tr("Select 2DA CSV Root Directory"),
        m_rootPath.isEmpty() ? QDir::homePath() : m_rootPath);

    if (!path.isEmpty()) {
        loadFolder(path);
    }
}

void MainWindow::onExport()
{
    if (m_currentFamily.isEmpty()) {
        QMessageBox::information(this, tr("Export"),
            tr("Please select a family first."));
        return;
    }

    QString fileName = QFileDialog::getSaveFileName(this,
        tr("Export Merged Table"),
        m_currentFamily + "_merged.csv",
        tr("CSV Files (*.csv);;JSON Files (*.json)"));

    if (fileName.isEmpty()) {
        return;
    }

    FfiResolvedTable* table = m_tablePanel->resolvedTable();
    if (!table) {
        QMessageBox::warning(this, tr("Export Error"),
            tr("No table data to export."));
        return;
    }

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Export Error"),
            tr("Could not open file for writing:\n%1").arg(file.errorString()));
        return;
    }

    FfiWrapper& ffi = FfiWrapper::instance();
    size_t colCount = ffi.tableColumnCount(table);
    size_t rowCount = ffi.tableRowCount(table);

    // Build column name list
    QStringList columnNames;
    for (size_t c = 0; c < colCount; ++c) {
        FfiColumnInfo* col = ffi.tableGetColumn(table, c);
        if (col && col->name) {
            columnNames << QString::fromUtf8(col->name);
            ffi.freeColumnInfo(col);
        } else {
            columnNames << QString();
        }
    }

    // Helper lambda to find pending edit value (returns empty QString if not found, with found flag)
    auto findPendingEdit = [this](int64_t rowId, const QString& colName) -> QPair<bool, QString> {
        for (const auto& edit : m_pendingEdits) {
            if (edit.rowId == rowId && edit.column == colName) {
                return qMakePair(true, edit.value);
            }
        }
        return qMakePair(false, QString());
    };

    int editsApplied = 0;

    if (fileName.endsWith(".json", Qt::CaseInsensitive)) {
        // Export as JSON
        QJsonObject root;
        root["family"] = m_currentFamily;

        // Columns
        QJsonArray columnsArray;
        for (const QString& colName : columnNames) {
            columnsArray.append(colName);
        }
        root["columns"] = columnsArray;

        // Rows
        QJsonArray rowsArray;
        for (size_t r = 0; r < rowCount; ++r) {
            QJsonObject rowObj;
            int64_t rowId = ffi.tableGetRowId(table, r);
            rowObj["id"] = rowId;

            QJsonObject cellsObj;
            for (size_t c = 0; c < colCount; ++c) {
                QString colName = columnNames[c];
                QString value;

                // Check for pending edit first (direct search)
                auto editResult = findPendingEdit(rowId, colName);
                if (editResult.first) {
                    value = editResult.second;
                    ++editsApplied;
                } else {
                    FfiResolvedCell* cell = ffi.tableGetCell(table, r, c);
                    if (cell) {
                        switch (cell->value.value_type) {
                            case 1: value = QString::number(cell->value.int_value); break;
                            case 2: value = QString::number(cell->value.float_value, 'g', 6); break;
                            case 3: if (cell->value.string_value) value = QString::fromUtf8(cell->value.string_value); break;
                            default: break;
                        }
                        ffi.freeCell(cell);
                    }
                }

                cellsObj[colName] = value;
            }
            rowObj["cells"] = cellsObj;
            rowsArray.append(rowObj);
        }
        root["rows"] = rowsArray;

        QJsonDocument doc(root);
        file.write(doc.toJson(QJsonDocument::Indented));
    } else {
        // Export as CSV
        QTextStream out(&file);

        // Write header
        out << columnNames.join(",") << "\n";

        // Write rows
        for (size_t r = 0; r < rowCount; ++r) {
            int64_t rowId = ffi.tableGetRowId(table, r);
            QStringList values;

            for (size_t c = 0; c < colCount; ++c) {
                QString colName = columnNames[c];
                QString value;

                // Check for pending edit first (direct search)
                auto editResult = findPendingEdit(rowId, colName);
                if (editResult.first) {
                    value = editResult.second;
                    ++editsApplied;
                } else {
                    FfiResolvedCell* cell = ffi.tableGetCell(table, r, c);
                    if (cell) {
                        switch (cell->value.value_type) {
                            case 1: value = QString::number(cell->value.int_value); break;
                            case 2: value = QString::number(cell->value.float_value, 'g', 6); break;
                            case 3: if (cell->value.string_value) value = QString::fromUtf8(cell->value.string_value); break;
                            default: break;
                        }
                        ffi.freeCell(cell);
                    }
                }

                values << escapeCsv(value);
            }
            out << values.join(",") << "\n";
        }
    }

    file.close();

    QString exportMsg = tr("Exported %1 rows to %2").arg(rowCount).arg(fileName);
    if (editsApplied > 0) {
        exportMsg += tr(" (applied %1 pending edits)").arg(editsApplied);
    }
    statusBar()->showMessage(exportMsg);
}

void MainWindow::onSavePatch()
{
    if (m_pendingEdits.isEmpty()) {
        QMessageBox::information(this, tr("Save Patch"),
            tr("No pending edits to save."));
        return;
    }

    if (m_currentFamily.isEmpty()) {
        QMessageBox::warning(this, tr("Save Patch"),
            tr("No family selected."));
        return;
    }

    QString fileName = QFileDialog::getSaveFileName(this,
        tr("Save Patch File"),
        m_currentFamily + "_patch.json",
        tr("JSON Files (*.json)"));

    if (fileName.isEmpty()) {
        return;
    }

    // Build patch JSON
    QJsonObject root;
    root["family"] = m_currentFamily;

    QJsonArray edits;
    for (const auto& edit : m_pendingEdits) {
        QJsonObject e;
        e["row_id"] = edit.rowId;
        e["column"] = edit.column;
        e["value"] = edit.value;
        edits.append(e);
    }
    root["edits"] = edits;

    // Write to file
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Save Error"),
            tr("Could not open file for writing:\n%1").arg(file.errorString()));
        return;
    }

    QJsonDocument doc(root);
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();

    statusBar()->showMessage(tr("Patch saved to %1 (%2 edits)")
        .arg(fileName).arg(m_pendingEdits.size()));
}

void MainWindow::onImportPatch()
{
    if (m_currentFamily.isEmpty()) {
        QMessageBox::information(this, tr("Import Patch"),
            tr("Please select a family first."));
        return;
    }

    QString fileName = QFileDialog::getOpenFileName(this,
        tr("Import Patch File"),
        QString(),
        tr("JSON Files (*.json)"));

    if (fileName.isEmpty()) {
        return;
    }

    // Read patch file
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Import Error"),
            tr("Could not open patch file:\n%1").arg(file.errorString()));
        return;
    }

    QByteArray data = file.readAll();
    file.close();

    // Parse JSON
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        QMessageBox::warning(this, tr("Import Error"),
            tr("Invalid JSON:\n%1").arg(parseError.errorString()));
        return;
    }

    QJsonObject root = doc.object();
    QString patchFamily = root["family"].toString();

    // Check if family matches
    if (patchFamily != m_currentFamily) {
        QMessageBox::StandardButton reply = QMessageBox::question(this,
            tr("Family Mismatch"),
            tr("This patch is for family '%1' but you have '%2' selected.\n\n"
               "Import anyway?").arg(patchFamily).arg(m_currentFamily),
            QMessageBox::Yes | QMessageBox::No);

        if (reply != QMessageBox::Yes) {
            return;
        }
    }

    // Get the table for cell updates
    FfiResolvedTable* table = m_tablePanel->resolvedTable();
    FfiWrapper& ffi = FfiWrapper::instance();

    // Import edits
    QJsonArray edits = root["edits"].toArray();
    int imported = 0;
    int updated = 0;

    for (const QJsonValue& editVal : edits) {
        QJsonObject editObj = editVal.toObject();
        int64_t rowId = static_cast<int64_t>(editObj["row_id"].toDouble());
        QString column = editObj["column"].toString();
        QString value = editObj["value"].toString();

        // Check if we already have an edit for this cell
        bool found = false;
        for (int i = 0; i < m_pendingEdits.size(); ++i) {
            if (m_pendingEdits[i].rowId == rowId && m_pendingEdits[i].column == column) {
                m_pendingEdits[i].value = value;
                found = true;
                ++updated;
                break;
            }
        }

        if (!found) {
            PendingEditInfo edit;
            edit.rowId = rowId;
            edit.column = column;
            edit.value = value;
            m_pendingEdits.append(edit);
            ++imported;
        }

        // Update the table cell display
        if (table) {
            size_t rowCount = ffi.tableRowCount(table);
            for (size_t r = 0; r < rowCount; ++r) {
                if (ffi.tableGetRowId(table, r) == rowId) {
                    size_t colCount = ffi.tableColumnCount(table);
                    for (size_t c = 0; c < colCount; ++c) {
                        FfiColumnInfo* col = ffi.tableGetColumn(table, c);
                        if (col && col->name && QString::fromUtf8(col->name) == column) {
                            m_tablePanel->updateCellValue(r, c, value);
                            ffi.freeColumnInfo(col);
                            break;
                        }
                        if (col) ffi.freeColumnInfo(col);
                    }
                    break;
                }
            }
        }
    }

    // Clear undo stack since we imported external edits
    m_undoStack.clear();

    // Update UI
    m_detailsPanel->updatePendingEdits(m_pendingEdits);
    updateWindowTitle();

    QString message = tr("Imported %1 edits").arg(imported);
    if (updated > 0) {
        message += tr(", updated %1 existing").arg(updated);
    }
    statusBar()->showMessage(message);
}

void MainWindow::onApplyPatch()
{
    if (!m_scanResult) {
        QMessageBox::warning(this, tr("Apply Patch"),
            tr("Please open a folder first."));
        return;
    }

    QString patchFilePath = QFileDialog::getOpenFileName(this,
        tr("Select Patch File"),
        QString(),
        tr("JSON Files (*.json)"));

    if (patchFilePath.isEmpty()) {
        return;
    }

    QString outputDir = QFileDialog::getExistingDirectory(this,
        tr("Select Output Directory for Modified Files"));

    if (outputDir.isEmpty()) {
        return;
    }

    // Read patch file
    QFile patchFile(patchFilePath);
    if (!patchFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Apply Patch"),
            tr("Could not open patch file:\n%1").arg(patchFile.errorString()));
        return;
    }
    QString patchJson = QString::fromUtf8(patchFile.readAll());
    patchFile.close();

    // Check if FFI is available
    FfiWrapper& ffi = FfiWrapper::instance();
    if (!ffi.isInitialized()) {
        QMessageBox::critical(this, tr("Apply Patch"),
            tr("FFI library not loaded. Cannot apply patch."));
        return;
    }

    // Validate patch
    statusBar()->showMessage(tr("Validating patch..."));
    QString validationError = ffi.validatePatch(m_scanResult, patchJson);

    if (!validationError.isEmpty()) {
        QMessageBox::warning(this, tr("Invalid Patch"),
            tr("Patch validation failed:\n%1").arg(validationError));
        return;
    }

    statusBar()->showMessage(tr("Applying patch..."));

    // Apply patch
    QString historyPath = QDir(m_rootPath).filePath("history.json");
    FfiPatchResult* result = ffi.applyPatch(m_scanResult, patchJson, outputDir, historyPath);

    if (!result) {
        QString error = ffi.lastError();
        QMessageBox::warning(this, tr("Apply Patch Failed"),
            tr("Failed to apply patch:\n%1").arg(error.isEmpty() ? tr("Unknown error") : error));
        return;
    }

    // Show results
    size_t exportCount = ffi.patchExportCount(result);
    QStringList exportedFiles;
    for (size_t i = 0; i < exportCount; ++i) {
        QString path = ffi.patchGetExportPath(result, i);
        if (!path.isEmpty()) {
            exportedFiles << QFileInfo(path).fileName();
        }
    }

    ffi.patchFree(result);

    QString message = tr("Patch applied successfully!\n\n"
                         "Files exported to:\n%1\n\n"
                         "Exported files:\n%2")
        .arg(outputDir)
        .arg(exportedFiles.isEmpty() ? tr("(none)") : exportedFiles.join("\n"));

    QMessageBox::information(this, tr("Patch Applied"), message);

    statusBar()->showMessage(tr("Patch applied, %1 files exported to %2")
        .arg(exportCount).arg(outputDir));
    onPatchApplied();
}

void MainWindow::onUndo()
{
    if (m_pendingEdits.isEmpty()) {
        statusBar()->showMessage(tr("Nothing to undo"));
        return;
    }

    PendingEditInfo last = m_pendingEdits.takeLast();
    m_undoStack.append(last);
    m_detailsPanel->updatePendingEdits(m_pendingEdits);
    updateWindowTitle();

    // Revert cell display to original value
    FfiResolvedTable* table = m_tablePanel->resolvedTable();
    if (table) {
        FfiWrapper& ffi = FfiWrapper::instance();
        // Find row index from rowId
        size_t rowCount = ffi.tableRowCount(table);
        for (size_t r = 0; r < rowCount; ++r) {
            if (ffi.tableGetRowId(table, r) == last.rowId) {
                // Find column index
                size_t colCount = ffi.tableColumnCount(table);
                for (size_t c = 0; c < colCount; ++c) {
                    FfiColumnInfo* col = ffi.tableGetColumn(table, c);
                    if (col && col->name && QString::fromUtf8(col->name) == last.column) {
                        m_tablePanel->revertCellValue(r, c);
                        ffi.freeColumnInfo(col);
                        break;
                    }
                    if (col) ffi.freeColumnInfo(col);
                }
                break;
            }
        }
    }

    statusBar()->showMessage(tr("Undid edit: Row %1, %2").arg(last.rowId).arg(last.column));
}

void MainWindow::onRedo()
{
    if (m_undoStack.isEmpty()) {
        statusBar()->showMessage(tr("Nothing to redo"));
        return;
    }

    PendingEditInfo edit = m_undoStack.takeLast();
    m_pendingEdits.append(edit);
    m_detailsPanel->updatePendingEdits(m_pendingEdits);
    updateWindowTitle();

    // Update cell display with the edited value
    FfiResolvedTable* table = m_tablePanel->resolvedTable();
    if (table) {
        FfiWrapper& ffi = FfiWrapper::instance();
        // Find row index from rowId
        size_t rowCount = ffi.tableRowCount(table);
        for (size_t r = 0; r < rowCount; ++r) {
            if (ffi.tableGetRowId(table, r) == edit.rowId) {
                // Find column index
                size_t colCount = ffi.tableColumnCount(table);
                for (size_t c = 0; c < colCount; ++c) {
                    FfiColumnInfo* col = ffi.tableGetColumn(table, c);
                    if (col && col->name && QString::fromUtf8(col->name) == edit.column) {
                        m_tablePanel->updateCellValue(r, c, edit.value);
                        ffi.freeColumnInfo(col);
                        break;
                    }
                    if (col) ffi.freeColumnInfo(col);
                }
                break;
            }
        }
    }

    statusBar()->showMessage(tr("Redid edit: Row %1, %2").arg(edit.rowId).arg(edit.column));
}

void MainWindow::onAbout()
{
    QMessageBox::about(this, tr("About DA Table Viewer"),
        tr("<h3>DA Table Viewer</h3>"
           "<p>Version 0.1.0</p>"
           "<p>A tool for viewing and editing Dragon Age 2DA table CSV files "
           "with provenance tracking.</p>"
           "<p>Built with Qt and Rust.</p>"));
}

void MainWindow::onFamilySelected(const QString& familyName)
{
    if (!m_scanResult) {
        return;
    }

    m_currentFamily = familyName;

    // Load and display merged table
    m_tablePanel->loadFamily(m_scanResult, familyName);

    // Update members list in family panel
    m_familyPanel->showMembers(m_scanResult, familyName);

    // Clear details panel
    m_detailsPanel->clear();

    updateWindowTitle();

    statusBar()->showMessage(tr("Loaded family: %1").arg(familyName));
}

void MainWindow::onCellSelected(int row, int col)
{
    // Update details panel with cell info
    m_detailsPanel->showCellDetails(m_tablePanel->resolvedTable(), row, col);
}

void MainWindow::onEditRequested(int64_t rowId, const QString& column, const QString& newValue)
{
    // Check if we already have an edit for this cell
    for (int i = 0; i < m_pendingEdits.size(); ++i) {
        if (m_pendingEdits[i].rowId == rowId && m_pendingEdits[i].column == column) {
            m_pendingEdits[i].value = newValue;
            m_detailsPanel->updatePendingEdits(m_pendingEdits);
            updateWindowTitle();
            return;
        }
    }

    // Add new edit
    PendingEditInfo edit;
    edit.rowId = rowId;
    edit.column = column;
    edit.value = newValue;
    m_pendingEdits.append(edit);

    m_detailsPanel->updatePendingEdits(m_pendingEdits);
    updateWindowTitle();

    statusBar()->showMessage(tr("Edit added: Row %1, Column %2")
        .arg(rowId).arg(column));
}

void MainWindow::onPatchApplied()
{
    // Refresh history tab
    m_detailsPanel->refreshHistory();
}
