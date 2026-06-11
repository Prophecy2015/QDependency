#pragma once

#include "session/modulenode.h"

#include <QColor>
#include <QIcon>

namespace ui {

// All icons are drawn with QPainter at runtime (crisp on the dark theme, no assets).
class IconFactory {
public:
    // 16px module status icon used in the tree and the module list
    static QIcon moduleIcon(bool isExe, session::ModuleStatus status, bool delayLoad,
                            bool forwarded, bool duplicate, bool warning);
    static QIcon moduleIcon(const session::ModuleNode &node, bool isRoot);
    static QIcon moduleIcon(const session::ModuleRecord &rec);

    // letter icon for the function panes ("C" import, "E" export, error)
    static QIcon letterIcon(QChar letter, const QColor &color, bool error = false);

    // Segoe MDL2 Assets glyph icon for toolbar / menus
    static QIcon glyphIcon(QChar glyph, const QColor &color = QColor(0xcc, 0xcc, 0xcc));

    static QIcon appIcon();
};

} // namespace ui
