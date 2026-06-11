#pragma once

#include <QString>

namespace core {

// Undecorates MSVC (?Foo@@YAXXZ), Itanium/GCC (_ZN3Foo...) and
// C stdcall/fastcall (_Foo@12 / @Foo@12) symbol names.
// Returns the input unchanged when it is not a decorated name.
QString demangleSymbol(const QString &name);

} // namespace core
