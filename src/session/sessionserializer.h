#pragma once

#include "session/modulenode.h"

namespace session {

// Saves / restores a full analysis result as JSON (replaces depends.exe .dwi).
bool saveSession(const AnalysisResult &result, const QString &filePath, QString *error);
AnalysisResultPtr loadSession(const QString &filePath, QString *error);

} // namespace session
