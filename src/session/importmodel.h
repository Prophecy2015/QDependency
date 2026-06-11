#pragma once

#include "session/modulenode.h"

#include <QAbstractTableModel>

namespace session {

// "Parent Imports" pane: functions the parent imports from the selected module.
class ImportModel : public QAbstractTableModel {
    Q_OBJECT
public:
    enum Column { ColIcon = 0, ColOrdinal, ColHint, ColFunction, ColEntryPoint, ColCount };
    enum { SortRole = Qt::UserRole + 1 };

    explicit ImportModel(QObject *parent = nullptr);

    void setNode(const ModuleNode *node);   // nullptr clears
    void setUndecorate(bool on);

    int rowCount(const QModelIndex &parent = {}) const override;
    int columnCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QVariant headerData(int section, Qt::Orientation o, int role) const override;

    const ImportRef *refAt(int row) const;

private:
    const ModuleNode *m_node = nullptr;
    bool m_undecorate = false;
};

// shared helpers for the function panes
QString formatOrdinal(int64_t value);                 // "123 (0x007B)" / "N/A"
QString formatEntryPoint(bool resolved, uint32_t rva, const QString &forward);
QString maybeDemangled(const QString &name, bool undecorate);

} // namespace session
