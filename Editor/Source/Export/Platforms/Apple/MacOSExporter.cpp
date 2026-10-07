// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Export/Platforms/Apple/MacOSExporter.hpp"
#include <json/json.hpp>

namespace Surge
{
    void MacOSExporter::SerializeSettings(nlohmann::json& out) const
    {
        out = nlohmann::json::object(); // Nothing to persist yet
    }

    void MacOSExporter::DeserializeSettings(const nlohmann::json&)
    {
    }

    void MacOSExporter::CollectIssues(Vector<String>& issues)
    {
        issues.push_back("macOS export is not supported yet");
    }

} // namespace Surge
