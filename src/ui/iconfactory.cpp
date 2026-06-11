#include "ui/iconfactory.h"

#include <QHash>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>

namespace ui {

namespace {

const QColor kBlue(0x75, 0xbe, 0xff);     // normal module
const QColor kTeal(0x4e, 0xc9, 0xb0);     // exe
const QColor kRed(0xf4, 0x87, 0x71);      // missing / invalid
const QColor kYellow(0xcc, 0xa7, 0x00);   // delay-load / warning
const QColor kGreen(0x89, 0xd1, 0x85);    // forwarded
const QColor kGray(0x6e, 0x6e, 0x6e);     // duplicate

QPixmap basePixmap(int size, qreal dpr)
{
    QPixmap pm(int(size * dpr), int(size * dpr));
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);
    return pm;
}

void drawModuleShape(QPainter &p, const QColor &color, bool isExe, bool filled)
{
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF rect(2.0, 3.0, 12.0, 10.5);
    QPen pen(color, 1.4);
    p.setPen(pen);
    p.setBrush(filled ? QBrush(color.darker(220)) : Qt::NoBrush);
    p.drawRoundedRect(rect, 2, 2);
    if (isExe) {
        // window title bar
        p.drawLine(QPointF(2.5, 6.0), QPointF(13.5, 6.0));
    } else {
        // dll "pins" on the left edge
        p.drawLine(QPointF(0.5, 6.0), QPointF(2.0, 6.0));
        p.drawLine(QPointF(0.5, 9.0), QPointF(2.0, 9.0));
        p.drawLine(QPointF(0.5, 12.0), QPointF(2.0, 12.0));
    }
}

void drawCenterText(QPainter &p, const QColor &color, const QString &text)
{
    QFont f = p.font();
    f.setPixelSize(9);
    f.setBold(true);
    p.setFont(f);
    p.setPen(color);
    p.drawText(QRectF(2.0, 3.0, 12.0, 10.5), Qt::AlignCenter, text);
}

void drawHourglass(QPainter &p)
{
    p.setRenderHint(QPainter::Antialiasing);
    QPainterPath path;
    path.moveTo(10.0, 9.5);
    path.lineTo(15.0, 9.5);
    path.lineTo(10.0, 15.5);
    path.lineTo(15.0, 15.5);
    path.closeSubpath();
    p.fillRect(QRectF(9.0, 8.5, 7.0, 8.0), QColor(0x1e, 0x1e, 0x1e));
    p.setPen(QPen(kYellow, 1.2));
    p.setBrush(Qt::NoBrush);
    p.drawPath(path);
}

void drawForwardArrow(QPainter &p)
{
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(QRectF(8.5, 9.0, 7.5, 7.0), QColor(0x1e, 0x1e, 0x1e));
    p.setPen(QPen(kGreen, 1.5));
    p.drawLine(QPointF(9.5, 12.5), QPointF(14.5, 12.5));
    p.drawLine(QPointF(12.0, 10.0), QPointF(14.5, 12.5));
    p.drawLine(QPointF(12.0, 15.0), QPointF(14.5, 12.5));
}

void drawWarningBadge(QPainter &p)
{
    p.setRenderHint(QPainter::Antialiasing);
    QPainterPath tri;
    tri.moveTo(12.5, 1.0);
    tri.lineTo(16.0, 7.0);
    tri.lineTo(9.0, 7.0);
    tri.closeSubpath();
    p.fillPath(tri, kYellow);
    p.setPen(QColor(0x1e, 0x1e, 0x1e));
    QFont f = p.font();
    f.setPixelSize(6);
    f.setBold(true);
    p.setFont(f);
    p.drawText(QRectF(9.0, 1.0, 7.0, 6.5), Qt::AlignCenter, QStringLiteral("!"));
}

} // namespace

QIcon IconFactory::moduleIcon(bool isExe, session::ModuleStatus status, bool delayLoad,
                              bool forwarded, bool duplicate, bool warning)
{
    const quint32 key = (isExe ? 1u : 0u) | (quint32(status) << 1) | (delayLoad ? 8u : 0u)
                        | (forwarded ? 16u : 0u) | (duplicate ? 32u : 0u)
                        | (warning ? 64u : 0u);
    static QHash<quint32, QIcon> cache;
    const auto it = cache.constFind(key);
    if (it != cache.constEnd())
        return *it;

    QIcon icon;
    for (const qreal dpr : {1.0, 2.0}) {
        QPixmap pm = basePixmap(16, dpr);
        QPainter p(&pm);

        if (status == session::ModuleStatus::Missing) {
            const QColor c = delayLoad ? kYellow : kRed;
            drawModuleShape(p, c, isExe, false);
            drawCenterText(p, c, QStringLiteral("?"));
        } else if (status == session::ModuleStatus::Invalid) {
            drawModuleShape(p, kRed, isExe, true);
            drawCenterText(p, kRed.lighter(120), QStringLiteral("!"));
        } else {
            const QColor base = duplicate ? kGray : (isExe ? kTeal : kBlue);
            drawModuleShape(p, base, isExe, !duplicate);
        }

        if (warning && status == session::ModuleStatus::Ok)
            drawWarningBadge(p);
        if (forwarded)
            drawForwardArrow(p);
        else if (delayLoad)
            drawHourglass(p);

        p.end();
        if (duplicate && status == session::ModuleStatus::Ok) {
            QPixmap faded = basePixmap(16, dpr);
            QPainter fp(&faded);
            fp.setOpacity(0.55);
            fp.drawPixmap(0, 0, pm);
            fp.end();
            pm = faded;
        }
        icon.addPixmap(pm);
    }
    cache.insert(key, icon);
    return icon;
}

QIcon IconFactory::moduleIcon(const session::ModuleNode &node, bool isRoot)
{
    const bool isExe = isRoot || (node.pe && node.pe->valid && !node.pe->isDll());
    const bool warning = node.cpuMismatch || node.hasMissingImports
                         || (node.pe && node.pe->isDotNet);
    return moduleIcon(isExe, node.status, node.delayLoad, node.forwarded,
                      node.duplicate, warning);
}

QIcon IconFactory::moduleIcon(const session::ModuleRecord &rec)
{
    const bool isExe = rec.pe && rec.pe->valid && !rec.pe->isDll();
    return moduleIcon(isExe, rec.status, rec.delayLoadOnly, false, false,
                      rec.cpuMismatch || rec.hasMissingImports);
}

QIcon IconFactory::letterIcon(QChar letter, const QColor &color, bool error)
{
    const quint32 key = quint32(letter.unicode()) | (quint32(color.rgb()) << 16)
                        | (error ? 0x80000000u : 0);
    static QHash<quint32, QIcon> cache;
    const auto it = cache.constFind(key);
    if (it != cache.constEnd())
        return *it;

    QIcon icon;
    for (const qreal dpr : {1.0, 2.0}) {
        QPixmap pm = basePixmap(16, dpr);
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing);
        const QColor c = error ? kRed : color;
        p.setPen(QPen(c, 1.2));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(QRectF(1.5, 1.5, 13.0, 13.0), 3, 3);
        QFont f = p.font();
        f.setPixelSize(9);
        f.setBold(true);
        p.setFont(f);
        p.drawText(QRectF(1.5, 1.5, 13.0, 13.0), Qt::AlignCenter,
                   error ? QStringLiteral("!") : QString(letter));
        p.end();
        icon.addPixmap(pm);
    }
    cache.insert(key, icon);
    return icon;
}

QIcon IconFactory::glyphIcon(QChar glyph, const QColor &color)
{
    const quint64 key = quint64(glyph.unicode()) | (quint64(color.rgb()) << 16);
    static QHash<quint64, QIcon> cache;
    const auto it = cache.constFind(key);
    if (it != cache.constEnd())
        return *it;

    QIcon icon;
    for (const qreal dpr : {1.0, 2.0}) {
        QPixmap pm = basePixmap(16, dpr);
        QPainter p(&pm);
        p.setRenderHint(QPainter::TextAntialiasing);
        QFont f(QStringLiteral("Segoe MDL2 Assets"));
        f.setPixelSize(13);
        p.setFont(f);
        p.setPen(color);
        p.drawText(QRect(0, 0, 16, 16), Qt::AlignCenter, QString(glyph));
        p.end();
        icon.addPixmap(pm);
    }
    cache.insert(key, icon);
    return icon;
}

QIcon IconFactory::appIcon()
{
    QIcon icon;
    for (const int size : {16, 32, 48, 256}) {
        QPixmap pm(size, size);
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing);
        const qreal s = size / 16.0;
        p.setPen(QPen(QColor(0x00, 0x7a, 0xcc), 1.6 * s));
        p.setBrush(QColor(0x1e, 0x1e, 0x1e));
        p.drawRoundedRect(QRectF(1.5 * s, 1.5 * s, 13.0 * s, 13.0 * s), 3 * s, 3 * s);
        p.setPen(QPen(kBlue, 1.4 * s));
        // tiny dependency tree glyph
        p.drawLine(QPointF(5 * s, 4.5 * s), QPointF(5 * s, 11.5 * s));
        p.drawLine(QPointF(5 * s, 6 * s), QPointF(8.5 * s, 6 * s));
        p.drawLine(QPointF(5 * s, 9 * s), QPointF(8.5 * s, 9 * s));
        p.drawLine(QPointF(5 * s, 11.5 * s), QPointF(8.5 * s, 11.5 * s));
        p.setBrush(kTeal);
        p.setPen(Qt::NoPen);
        p.drawEllipse(QPointF(5 * s, 4 * s), 1.4 * s, 1.4 * s);
        p.setBrush(kBlue);
        p.drawEllipse(QPointF(10 * s, 6 * s), 1.4 * s, 1.4 * s);
        p.drawEllipse(QPointF(10 * s, 9 * s), 1.4 * s, 1.4 * s);
        p.drawEllipse(QPointF(10 * s, 11.5 * s), 1.4 * s, 1.4 * s);
        p.end();
        icon.addPixmap(pm);
    }
    return icon;
}

} // namespace ui
