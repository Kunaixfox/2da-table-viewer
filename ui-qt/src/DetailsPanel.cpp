#include "DetailsPanel.h"
#include "FfiWrapper.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QFileInfo>
#include <QDir>
#include <QSettings>
#include <QFileDialog>

DetailsPanel::DetailsPanel(QWidget* parent)
    : QWidget(parent)
    , m_tabWidget(nullptr)
    , m_currentTable(nullptr)
    , m_selectedRow(-1)
    , m_selectedCol(-1)
    , m_selectedRowId(-1)
    , m_historyFile(nullptr)
{
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_tabWidget = new QTabWidget();
    layout->addWidget(m_tabWidget);

    setupProvenanceTab();
    setupEditTab();
    setupHistoryTab();
}

DetailsPanel::~DetailsPanel()
{
    if (m_historyFile) {
        FfiWrapper::instance().historyFree(m_historyFile);
    }
}

void DetailsPanel::setupProvenanceTab()
{
    m_provenanceTab = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(m_provenanceTab);

    // Cell info group
    QGroupBox* cellGroup = new QGroupBox(tr("Cell Info"));
    QVBoxLayout* cellLayout = new QVBoxLayout(cellGroup);

    m_cellLabel = new QLabel(tr("Cell: -"));
    cellLayout->addWidget(m_cellLabel);

    m_valueLabel = new QLabel(tr("Value: -"));
    m_valueLabel->setWordWrap(true);
    cellLayout->addWidget(m_valueLabel);

    m_typeLabel = new QLabel(tr("Type: -"));
    cellLayout->addWidget(m_typeLabel);

    layout->addWidget(cellGroup);

    // Source group
    QGroupBox* sourceGroup = new QGroupBox(tr("Source"));
    QVBoxLayout* sourceLayout = new QVBoxLayout(sourceGroup);

    m_sourceLabel = new QLabel(tr("File: -"));
    m_sourceLabel->setWordWrap(true);
    sourceLayout->addWidget(m_sourceLabel);

    m_lineLabel = new QLabel(tr("Line: -"));
    sourceLayout->addWidget(m_lineLabel);

    layout->addWidget(sourceGroup);

    // Override chain group
    QGroupBox* overrideGroup = new QGroupBox(tr("Override Chain"));
    QVBoxLayout* overrideLayout = new QVBoxLayout(overrideGroup);

    m_overrideTree = new QTreeWidget();
    m_overrideTree->setHeaderHidden(true);
    m_overrideTree->setMaximumHeight(150);
    overrideLayout->addWidget(m_overrideTree);

    layout->addWidget(overrideGroup);

    layout->addStretch();

    m_tabWidget->addTab(m_provenanceTab, tr("Provenance"));
}

void DetailsPanel::setupEditTab()
{
    m_editTab = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(m_editTab);

    // Edit cell group
    QGroupBox* editGroup = new QGroupBox(tr("Edit Cell"));
    QVBoxLayout* editLayout = new QVBoxLayout(editGroup);

    m_editRowLabel = new QLabel(tr("Row ID: -"));
    editLayout->addWidget(m_editRowLabel);

    m_editColLabel = new QLabel(tr("Column: -"));
    editLayout->addWidget(m_editColLabel);

    m_editCurrentLabel = new QLabel(tr("Current: -"));
    m_editCurrentLabel->setWordWrap(true);
    editLayout->addWidget(m_editCurrentLabel);

    QHBoxLayout* newLayout = new QHBoxLayout();
    QLabel* newLabel = new QLabel(tr("New:"));
    newLayout->addWidget(newLabel);
    m_editNewValue = new QLineEdit();
    newLayout->addWidget(m_editNewValue);
    editLayout->addLayout(newLayout);

    m_applyButton = new QPushButton(tr("Apply to Patch"));
    m_applyButton->setEnabled(false);
    editLayout->addWidget(m_applyButton);

    layout->addWidget(editGroup);

    // Pending changes group
    QGroupBox* pendingGroup = new QGroupBox(tr("Pending Changes"));
    QVBoxLayout* pendingLayout = new QVBoxLayout(pendingGroup);

    m_pendingList = new QListWidget();
    m_pendingList->setMaximumHeight(150);
    pendingLayout->addWidget(m_pendingList);

    m_clearButton = new QPushButton(tr("Clear All"));
    m_clearButton->setEnabled(false);
    pendingLayout->addWidget(m_clearButton);

    layout->addWidget(pendingGroup);

    layout->addStretch();

    // Connect signals
    connect(m_applyButton, &QPushButton::clicked, this, &DetailsPanel::onApplyEdit);
    connect(m_clearButton, &QPushButton::clicked, this, &DetailsPanel::onClearEdits);

    m_tabWidget->addTab(m_editTab, tr("Edit"));
}

void DetailsPanel::setupHistoryTab()
{
    m_historyTab = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(m_historyTab);

    QLabel* titleLabel = new QLabel(tr("Patch History"));
    titleLabel->setStyleSheet("font-weight: bold;");
    layout->addWidget(titleLabel);

    m_historyTree = new QTreeWidget();
    m_historyTree->setHeaderLabels({tr("Entry"), tr("Edits")});
    m_historyTree->setColumnWidth(0, 150);
    layout->addWidget(m_historyTree, 1);

    m_undoButton = new QPushButton(tr("Undo Selected"));
    m_undoButton->setEnabled(false);
    layout->addWidget(m_undoButton);

    // Connect signals
    connect(m_undoButton, &QPushButton::clicked, this, &DetailsPanel::onUndoHistoryEntry);
    connect(m_historyTree, &QTreeWidget::itemSelectionChanged, [this]() {
        m_undoButton->setEnabled(!m_historyTree->selectedItems().isEmpty());
    });

    m_tabWidget->addTab(m_historyTab, tr("History"));
}

void DetailsPanel::showCellDetails(FfiResolvedTable* table, int row, int col)
{
    m_currentTable = table;
    m_selectedRow = row;
    m_selectedCol = col;

    if (!table || row < 0 || col < 0) {
        clear();
        return;
    }

    FfiWrapper& ffi = FfiWrapper::instance();

    // Get column info
    size_t colCount = ffi.tableColumnCount(table);
    if ((size_t)col >= colCount) {
        // This is the _source column, no details
        m_cellLabel->setText(tr("Cell: _source"));
        m_valueLabel->setText(tr("Value: (derived)"));
        m_typeLabel->setText(tr("Type: -"));
        m_sourceLabel->setText(tr("File: -"));
        m_lineLabel->setText(tr("Line: -"));
        m_overrideTree->clear();
        m_applyButton->setEnabled(false);
        return;
    }

    QString columnName;
    FfiColumnInfo* colInfo = ffi.tableGetColumn(table, col);
    if (colInfo && colInfo->name) {
        columnName = QString::fromUtf8(colInfo->name);
        ffi.freeColumnInfo(colInfo);
    }

    // Get row ID
    m_selectedRowId = ffi.tableGetRowId(table, row);
    m_selectedColumn = columnName;

    // Get cell
    FfiResolvedCell* cell = ffi.tableGetCell(table, row, col);
    if (!cell) {
        clear();
        return;
    }

    // Update provenance tab
    m_cellLabel->setText(tr("Cell: %1").arg(columnName));

    QString value;
    QString type;
    switch (cell->value.value_type) {
        case 0:
            value = tr("(empty)");
            type = tr("Empty");
            break;
        case 1:
            value = QString::number(cell->value.int_value);
            type = tr("Integer");
            break;
        case 2:
            value = QString::number(cell->value.float_value, 'g', 6);
            type = tr("Float");
            break;
        case 3:
            if (cell->value.string_value) {
                value = QString::fromUtf8(cell->value.string_value);
            }
            type = tr("String");
            break;
    }

    m_valueLabel->setText(tr("Value: %1").arg(value));
    m_typeLabel->setText(tr("Type: %1").arg(type));

    QString source;
    if (cell->source_path) {
        source = QString::fromUtf8(cell->source_path);
        m_sourceLabel->setText(tr("File: %1").arg(QFileInfo(source).fileName()));
        m_sourceLabel->setToolTip(source);
    } else {
        m_sourceLabel->setText(tr("File: -"));
    }
    m_lineLabel->setText(tr("Line: -"));  // We don't track line numbers currently

    // TODO: Build override chain (would need additional FFI support)
    m_overrideTree->clear();
    if (!source.isEmpty()) {
        QTreeWidgetItem* item = new QTreeWidgetItem();
        item->setText(0, QFileInfo(source).fileName());
        item->setIcon(0, style()->standardIcon(QStyle::SP_ArrowRight));
        m_overrideTree->addTopLevelItem(item);

        QTreeWidgetItem* valItem = new QTreeWidgetItem(item);
        valItem->setText(0, QString("\"%1\"").arg(value));
        item->setExpanded(true);
    }

    // Update edit tab
    m_editRowLabel->setText(tr("Row ID: %1").arg(m_selectedRowId));
    m_editColLabel->setText(tr("Column: %1").arg(columnName));
    m_editCurrentLabel->setText(tr("Current: %1").arg(value));
    m_editNewValue->clear();
    m_applyButton->setEnabled(true);

    ffi.freeCell(cell);

    // Switch to provenance tab
    m_tabWidget->setCurrentWidget(m_provenanceTab);
}

void DetailsPanel::updatePendingEdits(const QList<PendingEditInfo>& edits)
{
    m_pendingList->clear();

    for (const auto& edit : edits) {
        QString text = tr("Row %1, %2 = \"%3\"")
            .arg(edit.rowId)
            .arg(edit.column)
            .arg(edit.value);
        m_pendingList->addItem(text);
    }

    m_clearButton->setEnabled(!edits.isEmpty());
}

void DetailsPanel::refreshHistory()
{
    loadHistory();
}

void DetailsPanel::clear()
{
    m_currentTable = nullptr;
    m_selectedRow = -1;
    m_selectedCol = -1;
    m_selectedRowId = -1;
    m_selectedColumn.clear();

    m_cellLabel->setText(tr("Cell: -"));
    m_valueLabel->setText(tr("Value: -"));
    m_typeLabel->setText(tr("Type: -"));
    m_sourceLabel->setText(tr("File: -"));
    m_lineLabel->setText(tr("Line: -"));
    m_overrideTree->clear();

    m_editRowLabel->setText(tr("Row ID: -"));
    m_editColLabel->setText(tr("Column: -"));
    m_editCurrentLabel->setText(tr("Current: -"));
    m_editNewValue->clear();
    m_applyButton->setEnabled(false);
}

void DetailsPanel::loadHistory()
{
    m_historyTree->clear();

    // Get history path from settings or use default
    QSettings settings;
    QString rootPath = settings.value("lastRootPath").toString();
    if (rootPath.isEmpty()) {
        return;
    }

    m_historyPath = QDir(rootPath).filePath("history.json");

    if (m_historyFile) {
        FfiWrapper::instance().historyFree(m_historyFile);
        m_historyFile = nullptr;
    }

    m_historyFile = FfiWrapper::instance().historyLoad(m_historyPath);
    if (!m_historyFile) {
        return;
    }

    FfiWrapper& ffi = FfiWrapper::instance();
    size_t count = ffi.historyCount(m_historyFile);

    for (size_t i = 0; i < count; ++i) {
        FfiHistoryEntry* entry = ffi.historyGetEntry(m_historyFile, i);
        if (!entry) continue;

        QTreeWidgetItem* item = new QTreeWidgetItem();

        QString family = entry->family ? QString::fromUtf8(entry->family) : tr("unknown");
        QString timestamp = entry->timestamp ? QString::fromUtf8(entry->timestamp) : tr("-");

        // Format: "family @ timestamp"
        item->setText(0, QString("%1\n%2").arg(family).arg(timestamp));
        item->setText(1, QString::number(entry->edit_count));
        item->setData(0, Qt::UserRole, (qulonglong)i);  // Store index

        m_historyTree->addTopLevelItem(item);

        ffi.freeHistoryEntry(entry);
    }
}

void DetailsPanel::onApplyEdit()
{
    if (m_selectedRowId < 0 || m_selectedColumn.isEmpty()) {
        return;
    }

    QString newValue = m_editNewValue->text();
    emit editRequested(m_selectedRowId, m_selectedColumn, newValue);

    m_editNewValue->clear();
}

void DetailsPanel::onClearEdits()
{
    m_pendingList->clear();
    m_clearButton->setEnabled(false);
    emit clearEditsRequested();
}

void DetailsPanel::onUndoHistoryEntry()
{
    QList<QTreeWidgetItem*> selected = m_historyTree->selectedItems();
    if (selected.isEmpty()) {
        return;
    }

    size_t index = selected.first()->data(0, Qt::UserRole).toULongLong();

    // Get history entry to find family name
    FfiHistoryEntry* entry = FfiWrapper::instance().historyGetEntry(m_historyFile, index);
    if (!entry) {
        return;
    }

    QString family = entry->family ? QString::fromUtf8(entry->family) : QString();
    FfiWrapper::instance().freeHistoryEntry(entry);

    if (family.isEmpty()) {
        return;
    }

    // Ask for output directory
    QString outputDir = QFileDialog::getExistingDirectory(this,
        tr("Select Output Directory for Restored Files"));

    if (outputDir.isEmpty()) {
        return;
    }

    emit undoHistoryRequested(family, outputDir);
}
