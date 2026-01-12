#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSplitter>
#include <QString>
#include <QList>
#include "DetailsPanel.h"  // For PendingEditInfo

class FamilyPanel;
class TablePanel;
struct FfiScanResult;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

private slots:
    void onOpenFolder();
    void onExport();
    void onSavePatch();
    void onImportPatch();
    void onApplyPatch();
    void onUndo();
    void onRedo();
    void onAbout();

    // Inter-panel communication
    void onFamilySelected(const QString& familyName);
    void onCellSelected(int row, int col);
    void onEditRequested(int64_t rowId, const QString& column, const QString& newValue);
    void onPatchApplied();

private:
    void createMenus();
    void createToolBar();
    void createStatusBar();
    void createPanels();
    void loadFolder(const QString& path);
    void updateWindowTitle();

    // Panels
    QSplitter* m_splitter;
    FamilyPanel* m_familyPanel;
    TablePanel* m_tablePanel;
    DetailsPanel* m_detailsPanel;

    // State
    QString m_rootPath;
    QString m_currentFamily;
    FfiScanResult* m_scanResult;

    // Pending edits (row_id, column, new_value)
    QList<PendingEditInfo> m_pendingEdits;
    QList<PendingEditInfo> m_undoStack;
};

#endif // MAINWINDOW_H
