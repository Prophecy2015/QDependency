#pragma once

#include "session/modulenode.h"

#include <QAbstractTableModel>
#include <QSet>

namespace session {

// "Exports" pane: all exports of the selected module; the ones actually
// imported by the parent are highlighted.
class ExportModel : public QAbstractTableModel {
    Q_OBJECT
public:
    enum Column { ColIcon = 0, ColOrdinal, ColHint, ColFunction, ColEntryPoint, ColCount };
    enum { SortRole = Qt::UserRole + 1 };

    explicit ExportModel(QObject *parent = nullptr);

    void setNode(const ModuleNode *node);
    void setUndecorate(bool on);

    int rowCount(const QModelIndex &parent = {}) const override;
    int columnCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QVariant headerData(int section, Qt::Orientation o, int role) const override;

    bool isUsedByParent(int row) const;
    const core::ExportEntry *entryAt(int row) const;

private:
    const ModuleNode *m_node = nullptr;
    QSet<uint32_t> m_usedOrdinals;
    bool m_undecorate = false;
};

} // namespace session
