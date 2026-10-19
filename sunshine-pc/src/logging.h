#pragma once

#include <filesystem>

namespace sunshine {
// Redirect ordinary launches to persistent logs. Preserve explicit file/pipe redirection.
// Returns the crash-report path for this session, or the legacy path for redirected runs.
std::filesystem::path initializeLogging();
}
