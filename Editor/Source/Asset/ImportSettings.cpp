// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Asset/ImportSettings.hpp"
#include "Surge/Utility/Filesystem.hpp"
#include <json/json.hpp>

namespace Surge
{
    static constexpr int IMPORT_SETTINGS_VERSION = 1;
    static constexpr const char* COLOR_SPACE_NAMES[] = { "AUTO", "SRGB", "LINEAR" };

    Path ImportSettings::GetPath(const Path& sourcePath)
    {
        Path path = sourcePath;
        path += EXTENSION;
        return path;
    }

    ImportSettings ImportSettings::Load(const Path& sourcePath)
    {
        ImportSettings settings;
        const Path path = GetPath(sourcePath);
        if(!Filesystem::Exists(path))
            return settings;

        String text;
        const nlohmann::json j = Filesystem::ReadTextFile(path, text) ? nlohmann::json::parse(text, nullptr, false) : nlohmann::json();
        if(j.is_discarded() || !j.is_object())
        {
            Log<Severity::Error>("[ImportSettings] {} is not valid JSON, using the defaults", path.string());
            return settings;
        }

        if(const auto mesh = j.find("Mesh"); mesh != j.end() && mesh->is_object())
        {
            if(const auto overrides = mesh->find("MaterialOverrides"); overrides != mesh->end() && overrides->is_object())
            {
                for(const auto& [slot, id] : overrides->items())
                {
                    char* end = nullptr;
                    const unsigned long index = std::strtoul(slot.c_str(), &end, 10);
                    if(end != slot.c_str() && *end == '\0' && id.is_number_unsigned() && id.get<uint64_t>() != AssetID::INVALID)
                        settings.MaterialOverrides[static_cast<Uint>(index)] = AssetID(id.get<uint64_t>());
                }
            }
        }

        if(const auto texture = j.find("Texture"); texture != j.end() && texture->is_object())
        {
            const String colorSpace = texture->value("ColorSpace", "AUTO");
            for(Uint i = 0; i < std::size(COLOR_SPACE_NAMES); i++)
            {
                if(colorSpace == COLOR_SPACE_NAMES[i])
                    settings.TextureColorSpace = static_cast<ColorSpace>(i);
            }
        }
        return settings;
    }

    bool ImportSettings::Save(const Path& sourcePath, const ImportSettings& settings)
    {
        const Path path = GetPath(sourcePath);
        if(settings.IsDefault())
        {
            if(Filesystem::Exists(path))
                Filesystem::RemoveFile(path);
            return true;
        }

        nlohmann::json j;
        j["Version"] = IMPORT_SETTINGS_VERSION;
        if(!settings.MaterialOverrides.empty())
        {
            nlohmann::json& overrides = j["Mesh"]["MaterialOverrides"];
            for(const auto& [slot, id] : settings.MaterialOverrides)
                overrides[std::to_string(slot)] = id.Get();
        }
        if(settings.TextureColorSpace != ColorSpace::AUTO)
            j["Texture"]["ColorSpace"] = COLOR_SPACE_NAMES[static_cast<Uint>(settings.TextureColorSpace)];

        if(!Filesystem::WriteTextFile(path, j.dump(4)))
        {
            Log<Severity::Error>("[ImportSettings] Failed to write {}", path.string());
            return false;
        }
        return true;
    }

} // namespace Surge
