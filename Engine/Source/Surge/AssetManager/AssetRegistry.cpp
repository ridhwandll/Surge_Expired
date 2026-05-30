// Copyright (c) - SurgeTechnologies - All rights reserved
#include "AssetRegistry.hpp"

namespace Surge
{
    static AssetMetadata sNullMetadata;

    bool AssetRegistry::Contains(const std::filesystem::path& path) const
    {
        return mRegistry.find(path) != mRegistry.end();;
    }

    size_t AssetRegistry::Remove(const std::filesystem::path& path)
    {
        return mRegistry.erase(path);
    }

    const AssetMetadata& AssetRegistry::Get(const std::filesystem::path& path) const
    {
        if (Contains(path))
        {
            return mRegistry.at(path);
        }
        Log<Severity::Error>("AssetRegistry: Asset with path '{}' not found in registry!", path.string());
        return sNullMetadata;
    }

    AssetMetadata& AssetRegistry::Get(const std::filesystem::path& path)
    {
        if (Contains(path))
        {
            return mRegistry.at(path);
        }
        Log<Severity::Error>("AssetRegistry: Asset with path '{}' not found in registry!", path.string());
        return sNullMetadata;
    }

    AssetMetadata& AssetRegistry::operator[](const std::filesystem::path& path)
    {
        SG_ASSERT(!path.string().empty(), "Path is empty!");
        return mRegistry[path];
    }
}
