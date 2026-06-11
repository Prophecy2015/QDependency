#pragma once

#include "session/modulenode.h"

#include <QObject>
#include <functional>

namespace session {

// Drives a full recursive dependency analysis on a background thread.
class AnalysisSession : public QObject {
    Q_OBJECT
public:
    explicit AnalysisSession(QObject *parent = nullptr);
    ~AnalysisSession() override;

    void start(const QString &filePath);
    bool isRunning() const { return m_running; }

signals:
    void progressText(const QString &text);
    void finished(session::AnalysisResultPtr result);

private:
    bool m_running = false;
};

// Synchronous analysis entry point (used by the worker thread and tests).
AnalysisResultPtr analyzeFile(const QString &filePath,
                              const std::function<void(QString)> &progress = {});

} // namespace session

Q_DECLARE_METATYPE(session::AnalysisResultPtr)
