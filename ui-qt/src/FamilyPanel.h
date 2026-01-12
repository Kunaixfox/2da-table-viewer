#ifndef FAMILYPANEL_H
#define FAMILYPANEL_H

#include <QWidget>
#include <QLineEdit>
#include <QListWidget>
#include <QTreeWidget>
#include <QString>

struct FfiScanResult;

class FamilyPanel : public QWidget
{
    Q_OBJECT

public:
    explicit FamilyPanel(QWidget* parent = nullptr);

    void loadFamilies(FfiScanResult* scanResult);
    void showMembers(FfiScanResult* scanResult, const QString& familyName);
    void clear();

signals:
    void familySelected(const QString& familyName);

private slots:
    void onSearchTextChanged(const QString& text);
    void onFamilyClicked(QListWidgetItem* item);

private:
    void populateFamilyList(const QStringList& families);

    QLineEdit* m_searchEdit;
    QListWidget* m_familyList;
    QTreeWidget* m_memberTree;

    FfiScanResult* m_scanResult;  // Not owned
    QStringList m_allFamilies;
};

#endif // FAMILYPANEL_H
