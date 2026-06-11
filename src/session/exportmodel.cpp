#include "session/exportmodel.h"
#include "session/importmodel.h"
#include "ui/iconfactory.h"

#include <QColor>

namespace session {

ExportModel::ExportModel(QObject *parent)
    : QAbstractTableModel(parent)
{
}

void ExportModel::setNode(const ModuleNode *node)
{
    beginResetModel();
    m_node = node;
    m_usedOrdinals.clear();
    if (node && node->pe) {
        for (const auto &ref : node->parentImports) {
            if (const auto *e = node->pe->findExport(ref.func))
                m_usedOrdinals.insert(e->ordinal);
        }
    }
    endResetModel();
}

void ExportModel::setUndecorate(bool on)
{
    if (m_undecorate == on)
        return;
    beginResetModel();
    m_undecorate = on;
    endResetModel();
}

int ExportModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() || !m_node || !m_node->pe ? 0 : int(m_node->pe->exports.size());
}

int ExportModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : ColCount;
}

const core::ExportEntry *ExportModel::entryAt(int row) const
{
    if (!m_node || !m_node->pe || row < 0 || row >= int(m_node->pe->exports.size()))
        return nullptr;
    return &m_node->pe->exports[row];
}

bool ExportModel::isUsedByParent(int row) const
{
    const auto *e = entryAt(row);
    return e && m_usedOrdinals.contains(e->ordinal);
}

QVariant ExportModel::data(const QModelIndex &index, int role) const
{
    const auto *e = entryAt(index.row());
    if (!e)
        return {};

    switch (role) {
    case Qt::DisplayRole:
        switch (index.column()) {
        case ColIcon:    return {};
        case ColOrdinal: return formatOrdinal(e->ordinal);
        case ColHint:    return formatOrdinal(e->hint);
        case ColFunction:
            return e->name.isEmpty()
                       ? QStringLiteral("N/A (Ordinal %1)").arg(e->ordinal)
                       : maybeDemangled(e->name, m_undecorate);
        case ColEntryPoint:
            return formatEntryPoint(true, e->rva, e->forward);
        }
        break;
    case Qt::DecorationRole:
        if (index.column() == ColIcon) {
            if (e->isForwarded())
                return ui::IconFactory::letterIcon(QLatin1Char('F'),
                                                   QColor(0x89, 0xd1, 0x85));
            return ui::IconFactory::letterIcon(QLatin1Char('E'),
                                               QColor(0x4e, 0xc9, 0xb0));
        }
        break;
    case Qt::ForegroundRole:
        if (index.column() == ColEntryPoint && e->isForwarded())
            return QColor(0x89, 0xd1, 0x85);
        if (isUsedByParent(index.row()))
            return QColor(0xdc, 0xdc, 0xaa);
        break;
    case SortRole:
        switch (index.column()) {
        case ColIcon:    return isUsedByParent(index.row()) ? 1 : 0;
        case ColOrdinal: return qint64(e->ordinal);
        case ColHint:    return qint64(e->hint);
        case ColFunction:
            return data(index.siblingAtColumn(ColFunction), Qt::DisplayRole);
        case ColEntryPoint:
            return e->isForwarded() ? e->forward : QString::number(e->rva, 16);
        }
        break;
    default:
        break;
    }
    return {};
}

QVariant ExportModel::headerData(int section, Qt::Orientation o, int role) const
{
    if (o != Qt::Horizontal || role != Qt::DisplayRole)
        return {};
    switch (section) {
    case ColIcon:       return QStringLiteral("E");
    case ColOrdinal:    return tr("Ordinal");
    case ColHint:       return tr("Hint");
    case ColFunction:   return tr("Function");
    case ColEntryPoint: return tr("Entry Point");
    }
    return {};
}

} // namespace session
