// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include <json/json.hpp> // Only include from .cpp files

namespace Surge::ExportSettingsIO
{
    // Reads j[key] into value if present and of the right type, otherwise keeps value (the default)
    template <typename T>
    void Read(const nlohmann::json& j, const char* key, T& value)
    {
        if(!j.is_object())
            return;

        const auto it = j.find(key);
        if(it == j.end() || it->is_null())
            return;

        try
        {
            value = it->get<T>();
        }
        catch(const nlohmann::json::exception&)
        {
        } // Wrong type: keep the default
    }

} // namespace Surge::ExportSettingsIO
