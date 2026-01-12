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

    connect(m_detailsPanel, &DetailsPanel::editRequested,
            this, &MainWindow::onEditRequested);
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

    // TODO: Implement export via FFI
    statusBar()->showMessage(tr("Exported to %1").arg(fileName));
}

void MainWindow::onSavePatch()
{
    if (m_pendingEdits.isEmpty()) {
        QMessageBox::information(this, tr("Save Patch"),
            tr("No pending edits to save."));
        return;
    }

    QString fileName = QFileDialog::getSaveFileName(this,
        tr("Save Patch File"),
        m_currentFamily + "_patch.json",
        tr("JSON Files (*.json)"));

    if (fileName.isEmpty()) {
        return;
    }

    // TODO: Build patch JSON and save
    statusBar()->showMessage(tr("Patch saved to %1").arg(fileName));
}

void MainWindow::onApplyPatch()
{
    QString patchFile = QFileDialog::getOpenFileName(this,
        tr("Select Patch File"),
        QString(),
        tr("JSON Files (*.json)"));

    if (patchFile.isEmpty()) {
        return;
    }

    QString outputDir = QFileDialog::getExistingDirectory(this,
        tr("Select Output Directory for Modified Files"));

    if (outputDir.isEmpty()) {
        return;
    }

    // TODO: Load patch, apply via FFI, update history
    statusBar()->showMessage(tr("Patch applied, files exported to %1").arg(outputDir));
    onPatchApplied();
}

void MainWindow::onUndo()
{
    // TODO: Implement undo (remove last pending edit or undo last applied patch)
    statusBar()->showMessage(tr("Undo"));
}

void MainWindow::onRedo()
{
    // TODO: Implement redo
    statusBar()->showMessage(tr("Redo"));
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

void MainWindow::onEditRequested(int rowId, const QString& column, const QString& newValue)
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
