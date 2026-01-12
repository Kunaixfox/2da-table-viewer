#ifndef TABLEPANEL_H
#define TABLEPANEL_H

#include <QWidget>
#include <QTableView>
#include <QStandardItemModel>
#include <QLineEdit>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QString>
#include <QFileInfo>

struct FfiScanResult;
struct FfiResolvedTable;

class TablePanel : public QWidget
{
    Q_OBJECT

public:
    explicit TablePanel(QWidget* parent = nullptr);
    ~TablePanel();

    void loadFamily(FfiScanResult* scanResult, const QString& familyName);
    void clear();
    void updateCellValue(int rowIndex, int colIndex, const QString& value);
    void revertCellValue(int rowIndex, int colIndex);

    FfiResolvedTable* resolvedTable() const { return m_resolvedTable; }
    QString currentFamily() const { return m_currentFamily; }

signals:
    void cellSelected(int row, int col);
    void cellEdited(int row, int col, const QString& newValue);

private slots:
    void onFilterTextChanged(const QString& text);
    void onFilterColumnChanged(int index);
    void onCellClicked(const QModelIndex& index);
    void onCellDataChanged(const QModelIndex& topLeft, const QModelIndex& bottomRight);
    void onPrevPage();
    void onNextPage();

private:
    void populateTable();
    void applyFilter();
    void updatePagination();

    // Header
    QLabel* m_familyLabel;

    // Table
    QTableView* m_tableView;
    QStandardItemModel* m_model;

    // Filter bar
    QLineEdit* m_filterEdit;
    QComboBox* m_columnCombo;

    // Pagination
    QLabel* m_pageLabel;
    QPushButton* m_prevButton;
    QPushButton* m_nextButton;

    // State
    FfiResolvedTable* m_resolvedTable;
    QString m_currentFamily;
    QString m_filterText;
    QString m_filterColumn;

    // Pagination state
    int m_currentPage;
    int m_rowsPerPage;
    int m_totalRows;
    QList<size_t> m_filteredIndices;  // Indices of rows matching filter
    bool m_updatingCell;  // Flag to prevent recursive dataChanged signals
};

#endif // TABLEPANEL_H
