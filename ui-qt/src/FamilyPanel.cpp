#include "FamilyPanel.h"
#include "FfiWrapper.h"

#include <QVBoxLayout>
#include <QLabel>
#include <QGroupBox>
#include <QFileInfo>
#include <QStyle>

FamilyPanel::FamilyPanel(QWidget* parent)
    : QWidget(parent)
    , m_searchEdit(nullptr)
    , m_familyList(nullptr)
    , m_memberTree(nullptr)
    , m_scanResult(nullptr)
{
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    // Families section
    QLabel* familiesLabel = new QLabel(tr("FAMILIES"));
    familiesLabel->setStyleSheet("font-weight: bold; padding: 4px;");
    layout->addWidget(familiesLabel);

    // Search box
    m_searchEdit = new QLineEdit();
    m_searchEdit->setPlaceholderText(tr("Search families..."));
    m_searchEdit->setClearButtonEnabled(true);
    layout->addWidget(m_searchEdit);

    // Family list
    m_familyList = new QListWidget();
    m_familyList->setAlternatingRowColors(true);
    layout->addWidget(m_familyList, 1);

    // Members section
    QLabel* membersLabel = new QLabel(tr("MEMBERS"));
    membersLabel->setStyleSheet("font-weight: bold; padding: 4px; margin-top: 8px;");
    layout->addWidget(membersLabel);

    // Member tree
    m_memberTree = new QTreeWidget();
    m_memberTree->setHeaderHidden(true);
    m_memberTree->setRootIsDecorated(false);
    m_memberTree->setMaximumHeight(150);
    layout->addWidget(m_memberTree);

    // Connect signals
    connect(m_searchEdit, &QLineEdit::textChanged,
            this, &FamilyPanel::onSearchTextChanged);

    connect(m_familyList, &QListWidget::itemClicked,
            this, &FamilyPanel::onFamilyClicked);
}

void FamilyPanel::loadFamilies(FfiScanResult* scanResult)
{
    m_scanResult = scanResult;
    m_allFamilies.clear();
    m_familyList->clear();
    m_memberTree->clear();

    if (!scanResult) {
        return;
    }

    FfiWrapper& ffi = FfiWrapper::instance();
    size_t count = ffi.scanFamilyCount(scanResult);

    for (size_t i = 0; i < count; ++i) {
        FfiFamilyInfo* info = ffi.scanGetFamily(scanResult, i);
        if (info && info->name) {
            QString name = QString::fromUtf8(info->name);
            QString display = QString("%1 (%2)").arg(name).arg(info->member_count);
            m_allFamilies.append(name);

            QListWidgetItem* item = new QListWidgetItem(display);
            item->setData(Qt::UserRole, name);  // Store actual name for selection
            m_familyList->addItem(item);

            ffi.freeFamilyInfo(info);
        }
    }
}

void FamilyPanel::showMembers(FfiScanResult* scanResult, const QString& familyName)
{
    m_memberTree->clear();

    if (!scanResult || familyName.isEmpty()) {
        return;
    }

    FfiWrapper& ffi = FfiWrapper::instance();
    size_t count = 0;
    FfiMemberInfo* members = ffi.scanGetMembers(scanResult, familyName, &count);

    if (members) {
        for (size_t i = 0; i < count; ++i) {
            FfiMemberInfo& m = members[i];
            QString path = m.path ? QString::fromUtf8(m.path) : QString();
            QString suffix = m.suffix ? QString::fromUtf8(m.suffix) : QString();

            // Extract just the filename
            QString filename = QFileInfo(path).fileName();

            QTreeWidgetItem* item = new QTreeWidgetItem();

            if (m.is_base) {
                item->setText(0, QString("%1 (base)").arg(filename));
                item->setIcon(0, style()->standardIcon(QStyle::SP_FileIcon));
            } else {
                item->setText(0, QString("%1 [%2]").arg(filename).arg(suffix));
                item->setIcon(0, style()->standardIcon(QStyle::SP_FileIcon));
            }

            item->setToolTip(0, path);  // Full path on hover
            item->setCheckState(0, Qt::Checked);

            m_memberTree->addTopLevelItem(item);
        }

        ffi.freeMemberInfoArray(members, count);
    }
}

void FamilyPanel::clear()
{
    m_scanResult = nullptr;
    m_allFamilies.clear();
    m_familyList->clear();
    m_memberTree->clear();
    m_searchEdit->clear();
}

void FamilyPanel::onSearchTextChanged(const QString& text)
{
    if (!m_scanResult) {
        return;
    }

    m_familyList->clear();

    if (text.isEmpty()) {
        // Show all families
        populateFamilyList(m_allFamilies);
    } else {
        // Search via FFI
        QStringList matches = FfiWrapper::instance().searchFamilies(m_scanResult, text);
        populateFamilyList(matches);
    }
}

void FamilyPanel::populateFamilyList(const QStringList& families)
{
    FfiWrapper& ffi = FfiWrapper::instance();

    for (const QString& name : families) {
        // Find member count
        size_t memberCount = 0;
        if (m_scanResult) {
            FfiMemberInfo* members = ffi.scanGetMembers(m_scanResult, name, &memberCount);
            if (members) {
                ffi.freeMemberInfoArray(members, memberCount);
            }
        }

        QString display = QString("%1 (%2)").arg(name).arg(memberCount);
        QListWidgetItem* item = new QListWidgetItem(display);
        item->setData(Qt::UserRole, name);
        m_familyList->addItem(item);
    }
}

void FamilyPanel::onFamilyClicked(QListWidgetItem* item)
{
    if (!item) {
        return;
    }

    QString familyName = item->data(Qt::UserRole).toString();
    emit familySelected(familyName);
}
