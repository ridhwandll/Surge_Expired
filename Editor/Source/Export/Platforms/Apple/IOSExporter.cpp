// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Export/Platforms/Apple/IOSExporter.hpp"
#include <json/json.hpp>

namespace Surge
{
    void IOSExporter::SerializeSettings(nlohmann::json& out) const
    {
        out = nlohmann::json::object(); // Nothing to persist yet
    }

    void IOSExporter::DeserializeSettings(const nlohmann::json&)
    {
    }

    void IOSExporter::CollectIssues(Vector<String>& issues)
    {
        issues.push_back("iOS export is not supported yet");
    }

} // namespace Surge
