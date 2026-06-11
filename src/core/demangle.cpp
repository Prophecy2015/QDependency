#include "core/demangle.h"

#include <QMutex>
#include <QRegularExpression>

#include <windows.h>
#include <dbghelp.h>

#include <cxxabi.h>
#include <cstdlib>

namespace core {

QString demangleSymbol(const QString &name)
{
    if (name.isEmpty())
        return name;

    if (name.startsWith(QLatin1Char('?'))) {
        // UnDecorateSymbolName is not thread-safe
        static QMutex mutex;
        QMutexLocker locker(&mutex);
        const QByteArray in = name.toLatin1();
        char out[4096];
        if (UnDecorateSymbolName(in.constData(), out, sizeof(out), UNDNAME_COMPLETE) > 0)
            return QString::fromLatin1(out);
        return name;
    }

    if (name.startsWith(QLatin1String("_Z"))) {
        const QByteArray in = name.toUtf8();
        int status = 0;
        char *res = abi::__cxa_demangle(in.constData(), nullptr, nullptr, &status);
        if (status == 0 && res) {
            const QString out = QString::fromUtf8(res);
            std::free(res);
            return out;
        }
        std::free(res);
        return name;
    }

    static const QRegularExpression stdcallRe(
        QStringLiteral("^[_@]([A-Za-z_][A-Za-z0-9_]*)@\\d+$"));
    const auto m = stdcallRe.match(name);
    if (m.hasMatch())
        return m.captured(1);

    return name;
}

} // namespace core
