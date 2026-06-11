#include "session/importmodel.h"

#include "core/demangle.h"
#include "ui/iconfactory.h"

#include <QColor>
#include <QHash>

namespace session {

QString formatOrdinal(int64_t value)
{
    if (value < 0)
        return QStringLiteral("N/A");
    return QStringLiteral("%1 (0x%2)")
        .arg(value)
        .arg(value, 4, 16, QLatin1Char('0'));
}

QString formatEntryPoint(bool resolved, uint32_t rva, const QString &forward)
{
    if (!resolved)
        return QStringLiteral("Not Found");
    if (!forward.isEmpty())
        return forward;
    if (rva == 0)
        return QStringLiteral("N/A");
    return QStringLiteral("0x%1").arg(rva, 8, 16, QLatin1Char('0')).toUpper()
        .replace(QLatin1String("0X"), QLatin1String("0x"));
}

QString maybeDemangled(const QString &name, bool undecorate)
{
    if (!undecorate || name.isEmpty())
        return name;
    static QHash<QString, QString> cache;
    const auto it = cache.constFind(name);
    if (it != cache.constEnd())
        return *it;
    const QString out = core::demangleSymbol(name);
    if (cache.size() > 100000)
        cache.clear();
    cache.insert(name, out);
    return out;
}

ImportModel::ImportModel(QObject *parent)
    : QAbstractTableModel(parent)
{
}

void ImportModel::setNode(const ModuleNode *node)
{
    beginResetModel();
    m_node = node;
    endResetModel();
}

void ImportModel::setUndecorate(bool on)
{
    if (m_undecorate == on)
        return;
    beginResetModel();
    m_undecorate = on;
    endResetModel();
}

int ImportModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() || !m_node ? 0 : int(m_node->parentImports.size());
}

int ImportModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : ColCount;
}

const ImportRef *ImportModel::refAt(int row) const
{
    if (!m_node || row < 0 || row >= int(m_node->parentImports.size()))
        return nullptr;
    return &m_node->parentImports[row];
}

QVariant ImportModel::data(const QModelIndex &index, int role) const
{
    const ImportRef *ref = refAt(index.row());
    if (!ref)
        return {};
    const auto &f = ref->func;

    switch (role) {
    case Qt::DisplayRole:
        switch (index.column()) {
        case ColIcon:       return {};
        case ColOrdinal:    return formatOrdinal(f.byOrdinal ? int64_t(f.ordinal) : -1);
        case ColHint:       return formatOrdinal(f.hint);
        case ColFunction:
            return f.byOrdinal ? f.displayName() : maybeDemangled(f.name, m_undecorate);
        case ColEntryPoint: return formatEntryPoint(ref->resolved, ref->entryRva, ref->forward);
        }
        break;
    case SortRole:
        switch (index.column()) {
        case ColIcon:       return ref->resolved ? 1 : 0;
        case ColOrdinal:    return qint64(f.byOrdinal ? f.ordinal : -1);
        case ColHint:       return qint64(f.hint);
        case ColFunction:   return data(index.siblingAtColumn(ColFunction), Qt::DisplayRole);
        case ColEntryPoint: return ref->forward.isEmpty() ? QString::number(ref->entryRva, 16)
                                                          : ref->forward;
        }
        break;
    case Qt::DecorationRole:
        if (index.column() == ColIcon) {
            if (!ref->resolved)
                return ui::IconFactory::letterIcon(QLatin1Char('!'), {}, true);
            return ui::IconFactory::letterIcon(f.byOrdinal ? QLatin1Char('O')
                                                           : QLatin1Char('C'),
                                               QColor(0x75, 0xbe, 0xff));
        }
        break;
    case Qt::ForegroundRole:
        if (!ref->resolved)
            return QColor(0xf4, 0x87, 0x71);
        if (index.column() == ColEntryPoint && !ref->forward.isEmpty())
            return QColor(0x89, 0xd1, 0x85);
        break;
    default:
        break;
    }
    return {};
}

QVariant ImportModel::headerData(int section, Qt::Orientation o, int role) const
{
    if (o != Qt::Horizontal || role != Qt::DisplayRole)
        return {};
    switch (section) {
    case ColIcon:       return QStringLiteral("PI");
    case ColOrdinal:    return tr("Ordinal");
    case ColHint:       return tr("Hint");
    case ColFunction:   return tr("Function");
    case ColEntryPoint: return tr("Entry Point");
    }
    return {};
}

} // namespace session
