#ifndef DETAILSPANEL_H
#define DETAILSPANEL_H

#include <QWidget>
#include <QTabWidget>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QListWidget>
#include <QTreeWidget>
#include <QString>
#include <QList>

struct FfiResolvedTable;
struct FfiHistoryFile;

// Forward declaration for pending edit
struct PendingEditInfo {
    int rowId;
    QString column;
    QString value;
};

class DetailsPanel : public QWidget
{
    Q_OBJECT

public:
    explicit DetailsPanel(QWidget* parent = nullptr);
    ~DetailsPanel();

    void showCellDetails(FfiResolvedTable* table, int row, int col);
    void updatePendingEdits(const QList<PendingEditInfo>& edits);
    void refreshHistory();
    void clear();

signals:
    void editRequested(int rowId, const QString& column, const QString& newValue);

private slots:
    void onApplyEdit();
    void onClearEdits();
    void onUndoHistoryEntry();

private:
    void setupProvenanceTab();
    void setupEditTab();
    void setupHistoryTab();
    void loadHistory();

    QTabWidget* m_tabWidget;

    // Provenance tab
    QWidget* m_provenanceTab;
    QLabel* m_cellLabel;
    QLabel* m_valueLabel;
    QLabel* m_typeLabel;
    QLabel* m_sourceLabel;
    QLabel* m_lineLabel;
    QTreeWidget* m_overrideTree;

    // Edit tab
    QWidget* m_editTab;
    QLabel* m_editRowLabel;
    QLabel* m_editColLabel;
    QLabel* m_editCurrentLabel;
    QLineEdit* m_editNewValue;
    QPushButton* m_applyButton;
    QListWidget* m_pendingList;
    QPushButton* m_clearButton;

    // History tab
    QWidget* m_historyTab;
    QTreeWidget* m_historyTree;
    QPushButton* m_undoButton;

    // State
    FfiResolvedTable* m_currentTable;  // Not owned
    int m_selectedRow;
    int m_selectedCol;
    int64_t m_selectedRowId;
    QString m_selectedColumn;
    QString m_historyPath;
    FfiHistoryFile* m_historyFile;
};

#endif // DETAILSPANEL_H
