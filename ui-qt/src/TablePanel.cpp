#include "TablePanel.h"
#include "FfiWrapper.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QHeaderView>

TablePanel::TablePanel(QWidget* parent)
    : QWidget(parent)
    , m_familyLabel(nullptr)
    , m_tableView(nullptr)
    , m_model(nullptr)
    , m_filterEdit(nullptr)
    , m_columnCombo(nullptr)
    , m_pageLabel(nullptr)
    , m_prevButton(nullptr)
    , m_nextButton(nullptr)
    , m_resolvedTable(nullptr)
    , m_currentPage(0)
    , m_rowsPerPage(50)
    , m_totalRows(0)
{
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    // Header
    m_familyLabel = new QLabel(tr("Select a family"));
    m_familyLabel->setStyleSheet("font-weight: bold; font-size: 14px; padding: 4px;");
    layout->addWidget(m_familyLabel);

    // Table view
    m_tableView = new QTableView();
    m_tableView->setAlternatingRowColors(true);
    m_tableView->setSelectionBehavior(QAbstractItemView::SelectItems);
    m_tableView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tableView->horizontalHeader()->setStretchLastSection(true);
    m_tableView->verticalHeader()->setDefaultSectionSize(24);

    m_model = new QStandardItemModel(this);
    m_tableView->setModel(m_model);

    layout->addWidget(m_tableView, 1);

    // Filter bar
    QHBoxLayout* filterLayout = new QHBoxLayout();

    QLabel* filterLabel = new QLabel(tr("Filter:"));
    filterLayout->addWidget(filterLabel);

    m_filterEdit = new QLineEdit();
    m_filterEdit->setPlaceholderText(tr("Enter search text..."));
    m_filterEdit->setClearButtonEnabled(true);
    filterLayout->addWidget(m_filterEdit, 1);

    QLabel* colLabel = new QLabel(tr("Column:"));
    filterLayout->addWidget(colLabel);

    m_columnCombo = new QComboBox();
    m_columnCombo->setMinimumWidth(120);
    filterLayout->addWidget(m_columnCombo);

    layout->addLayout(filterLayout);

    // Pagination bar
    QHBoxLayout* pageLayout = new QHBoxLayout();

    m_pageLabel = new QLabel();
    pageLayout->addWidget(m_pageLabel);

    pageLayout->addStretch();

    m_prevButton = new QPushButton(tr("<"));
    m_prevButton->setMaximumWidth(40);
    m_prevButton->setEnabled(false);
    pageLayout->addWidget(m_prevButton);

    m_nextButton = new QPushButton(tr(">"));
    m_nextButton->setMaximumWidth(40);
    m_nextButton->setEnabled(false);
    pageLayout->addWidget(m_nextButton);

    layout->addLayout(pageLayout);

    // Connect signals
    connect(m_filterEdit, &QLineEdit::textChanged,
            this, &TablePanel::onFilterTextChanged);

    connect(m_columnCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &TablePanel::onFilterColumnChanged);

    connect(m_tableView, &QTableView::clicked,
            this, &TablePanel::onCellClicked);

    connect(m_prevButton, &QPushButton::clicked,
            this, &TablePanel::onPrevPage);

    connect(m_nextButton, &QPushButton::clicked,
            this, &TablePanel::onNextPage);
}

TablePanel::~TablePanel()
{
    if (m_resolvedTable) {
        FfiWrapper::instance().tableFree(m_resolvedTable);
    }
}

void TablePanel::loadFamily(FfiScanResult* scanResult, const QString& familyName)
{
    // Free previous table
    if (m_resolvedTable) {
        FfiWrapper::instance().tableFree(m_resolvedTable);
        m_resolvedTable = nullptr;
    }

    m_currentFamily = familyName;
    m_familyLabel->setText(familyName);

    // Merge family
    m_resolvedTable = FfiWrapper::instance().mergeFamily(scanResult, familyName);

    if (!m_resolvedTable) {
        QString error = FfiWrapper::instance().lastError();
        m_familyLabel->setText(tr("Error: %1").arg(error));
        m_model->clear();
        return;
    }

    // Reset state
    m_currentPage = 0;
    m_filterText.clear();
    m_filterColumn.clear();
    m_filteredIndices.clear();
    m_filterEdit->clear();

    // Populate column combo
    m_columnCombo->clear();
    m_columnCombo->addItem(tr("All"), QString());

    FfiWrapper& ffi = FfiWrapper::instance();
    size_t colCount = ffi.tableColumnCount(m_resolvedTable);

    for (size_t i = 0; i < colCount; ++i) {
        FfiColumnInfo* col = ffi.tableGetColumn(m_resolvedTable, i);
        if (col && col->name) {
            QString name = QString::fromUtf8(col->name);
            m_columnCombo->addItem(name, name);
            ffi.freeColumnInfo(col);
        }
    }

    // Populate table
    populateTable();
}

void TablePanel::clear()
{
    if (m_resolvedTable) {
        FfiWrapper::instance().tableFree(m_resolvedTable);
        m_resolvedTable = nullptr;
    }

    m_currentFamily.clear();
    m_familyLabel->setText(tr("Select a family"));
    m_model->clear();
    m_columnCombo->clear();
    m_filterEdit->clear();
    m_filteredIndices.clear();
    m_currentPage = 0;
    m_totalRows = 0;
    updatePagination();
}

void TablePanel::populateTable()
{
    m_model->clear();

    if (!m_resolvedTable) {
        return;
    }

    FfiWrapper& ffi = FfiWrapper::instance();
    size_t colCount = ffi.tableColumnCount(m_resolvedTable);
    size_t rowCount = ffi.tableRowCount(m_resolvedTable);

    // Set up headers
    QStringList headers;
    for (size_t i = 0; i < colCount; ++i) {
        FfiColumnInfo* col = ffi.tableGetColumn(m_resolvedTable, i);
        if (col && col->name) {
            headers << QString::fromUtf8(col->name);
            ffi.freeColumnInfo(col);
        }
    }
    // Add source column
    headers << tr("_source");
    m_model->setHorizontalHeaderLabels(headers);

    // Determine which rows to show
    QList<size_t> rowsToShow;
    if (m_filteredIndices.isEmpty() && m_filterText.isEmpty()) {
        // No filter, show all rows for current page
        for (size_t i = 0; i < rowCount; ++i) {
            rowsToShow.append(i);
        }
    } else {
        rowsToShow = m_filteredIndices;
    }

    m_totalRows = rowsToShow.size();

    // Calculate page bounds
    int startRow = m_currentPage * m_rowsPerPage;
    int endRow = qMin(startRow + m_rowsPerPage, m_totalRows);

    // Populate rows
    for (int i = startRow; i < endRow; ++i) {
        size_t rowIndex = rowsToShow[i];
        QList<QStandardItem*> items;

        // Track source for the row (use first non-empty cell's source)
        QString rowSource;

        for (size_t c = 0; c < colCount; ++c) {
            FfiResolvedCell* cell = ffi.tableGetCell(m_resolvedTable, rowIndex, c);

            QString value;
            QString source;

            if (cell) {
                // Get value based on type
                switch (cell->value.value_type) {
                    case 0:  // Empty
                        value = "";
                        break;
                    case 1:  // Integer
                        value = QString::number(cell->value.int_value);
                        break;
                    case 2:  // Float
                        value = QString::number(cell->value.float_value, 'g', 6);
                        break;
                    case 3:  // String
                        if (cell->value.string_value) {
                            value = QString::fromUtf8(cell->value.string_value);
                        }
                        break;
                }

                if (cell->source_path) {
                    source = QString::fromUtf8(cell->source_path);
                    if (rowSource.isEmpty() && !value.isEmpty()) {
                        rowSource = QFileInfo(source).fileName();
                    }
                }

                ffi.freeCell(cell);
            }

            QStandardItem* item = new QStandardItem(value);
            item->setData(source, Qt::UserRole);  // Store full source path
            item->setEditable(false);
            items.append(item);
        }

        // Add source column
        QStandardItem* sourceItem = new QStandardItem(rowSource);
        sourceItem->setEditable(false);
        sourceItem->setForeground(Qt::gray);
        items.append(sourceItem);

        m_model->appendRow(items);
    }

    // Resize columns to content
    m_tableView->resizeColumnsToContents();

    updatePagination();
}

void TablePanel::applyFilter()
{
    m_filteredIndices.clear();

    if (m_filterText.isEmpty()) {
        // No filter
        m_currentPage = 0;
        populateTable();
        return;
    }

    if (!m_resolvedTable) {
        return;
    }

    FfiWrapper& ffi = FfiWrapper::instance();

    if (m_filterColumn.isEmpty()) {
        // Search all columns - do this manually
        size_t rowCount = ffi.tableRowCount(m_resolvedTable);
        size_t colCount = ffi.tableColumnCount(m_resolvedTable);

        for (size_t r = 0; r < rowCount; ++r) {
            bool match = false;
            for (size_t c = 0; c < colCount && !match; ++c) {
                FfiResolvedCell* cell = ffi.tableGetCell(m_resolvedTable, r, c);
                if (cell && cell->value.value_type == 3 && cell->value.string_value) {
                    QString val = QString::fromUtf8(cell->value.string_value);
                    if (val.contains(m_filterText, Qt::CaseInsensitive)) {
                        match = true;
                    }
                }
                if (cell) ffi.freeCell(cell);
            }
            if (match) {
                m_filteredIndices.append(r);
            }
        }
    } else {
        // Use FFI filter for specific column
        m_filteredIndices = ffi.tableFilterRows(m_resolvedTable, m_filterColumn, m_filterText);
    }

    m_currentPage = 0;
    populateTable();
}

void TablePanel::updatePagination()
{
    int totalPages = (m_totalRows + m_rowsPerPage - 1) / m_rowsPerPage;
    if (totalPages == 0) totalPages = 1;

    int startRow = m_currentPage * m_rowsPerPage + 1;
    int endRow = qMin(startRow + m_rowsPerPage - 1, m_totalRows);

    if (m_totalRows == 0) {
        m_pageLabel->setText(tr("No rows"));
    } else {
        m_pageLabel->setText(tr("Showing %1-%2 of %3")
            .arg(startRow).arg(endRow).arg(m_totalRows));
    }

    m_prevButton->setEnabled(m_currentPage > 0);
    m_nextButton->setEnabled(m_currentPage < totalPages - 1);
}

void TablePanel::onFilterTextChanged(const QString& text)
{
    m_filterText = text;
    applyFilter();
}

void TablePanel::onFilterColumnChanged(int index)
{
    m_filterColumn = m_columnCombo->itemData(index).toString();
    if (!m_filterText.isEmpty()) {
        applyFilter();
    }
}

void TablePanel::onCellClicked(const QModelIndex& index)
{
    if (!index.isValid()) {
        return;
    }

    // Calculate actual row index
    int displayRow = index.row();
    int actualRow;

    if (m_filteredIndices.isEmpty() && m_filterText.isEmpty()) {
        actualRow = m_currentPage * m_rowsPerPage + displayRow;
    } else {
        int pageOffset = m_currentPage * m_rowsPerPage + displayRow;
        if (pageOffset < m_filteredIndices.size()) {
            actualRow = m_filteredIndices[pageOffset];
        } else {
            return;
        }
    }

    emit cellSelected(actualRow, index.column());
}

void TablePanel::onPrevPage()
{
    if (m_currentPage > 0) {
        --m_currentPage;
        populateTable();
    }
}

void TablePanel::onNextPage()
{
    int totalPages = (m_totalRows + m_rowsPerPage - 1) / m_rowsPerPage;
    if (m_currentPage < totalPages - 1) {
        ++m_currentPage;
        populateTable();
    }
}
