// Copyright (c) - SurgeTechnologies - All rights reserved
#include "RegistryFormat.hpp"
#include "Surge/Asset/Format/BinarySerialization.hpp"
#include "Surge/Asset/Format/CookedFile.hpp"

namespace Surge
{
    SURGE_RAPTOR_FIELDS(AssetMetadata, v.ID, v.Type, v.RelativePath)

    namespace RegistryBinary
    {
        bool Write(const Path& path, const std::unordered_map<AssetID, AssetMetadata>& registry)
        {
            Rapt0R::Writer writer(registry.size() * 64 + 64);
            writer.Write(static_cast<Uint>(registry.size()));
            for(const auto& [id, metadata] : registry)
                writer.Write(metadata);

            CookedHeader header;
            header.Type = AssetType::NONE;
            return !writer.HasError() && CookedFileIO::Write(path, header, VERSION, writer.WrittenSpan());
        }

        bool Read(const Path& path, Vector<AssetMetadata>& outEntries)
        {
            CookedFile file;
            const CookedFileError error = CookedFileIO::Read(path, AssetType::NONE, VERSION, file);
            if(error != CookedFileError::NONE)
            {
                if(error != CookedFileError::UNREADABLE)
                    Log<Severity::Error>("[RegistryBinary] {} can't be read: {}", path.string(), CookedFileErrorToString(error));
                return false;
            }

            Rapt0R::Reader reader = file.GetPayload();
            if(!reader.Read(outEntries))
            {
                Log<Severity::Error>("[RegistryBinary] {} is truncated or corrupted!", path.string());
                outEntries.clear();
                return false;
            }

            for(const AssetMetadata& entry : outEntries)
            {
                if(!entry.ID.IsValid() || !IsEnumInRange(entry.Type, AssetType::PARTICLE_EFFECT) || entry.Type == AssetType::NONE || entry.RelativePath.empty())
                {
                    Log<Severity::Error>("[RegistryBinary] {} has an invalid entry ({})", path.string(), entry.RelativePath);
                    outEntries.clear();
                    return false;
                }
            }
            return true;
        }
    }

} // namespace Surge
