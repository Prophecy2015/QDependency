#pragma once

#include <QSet>
#include <QString>

namespace core {

// Lowercase file names listed under
// HKLM\SYSTEM\CurrentControlSet\Control\Session Manager\KnownDLLs.
const QSet<QString> &knownDlls();

} // namespace core
