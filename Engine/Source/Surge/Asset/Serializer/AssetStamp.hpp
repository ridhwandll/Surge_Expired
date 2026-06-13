// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Defines.hpp"
#include "Surge/Core/String.hpp"
#include "Surge/Utility/Filesystem.hpp"
#include <chrono>

namespace Surge
{
    // Prepended to every binary asset file (.rMESH, .rTEXTURE2D, .rMATERIAL)
    // Serializers respect this AssetStamp and expects it to be present at the start of the file
    // Serializers DO NOT WRITE the AssetStamp on Serialize

    // Example: Mesh:
    // AssetStamp written by   : MeshCooker             (Editor) fresh stamp, Current mod time
    // AssetStamp validated by : AssetCooker::NeedsCook (Editor) 16byte partial read
    // AssetStamp preserved by : MeshSerializer         (Engine) Read + Rewrite unchanged (MeshSerializer never interprets the stamp, it just carries it through)

    struct AssetStamp
    {
        static constexpr Uint kMagic = 0x4D545343; // CSTM

        Uint Magic = kMagic;
        Uint CookerVersion = 0;
        uint64_t SourceModTime = 0; // Filesystem Last Write Time (seconds)
    };
    static_assert(sizeof(AssetStamp) == 16);

    namespace AssetStampWriter
    {
        inline uint64_t GetModTime(const String& absPath)
        {
            std::error_code ec;
            auto ftime = std::filesystem::last_write_time(absPath, ec);
            if(ec)
                return 0;
            return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(ftime.time_since_epoch()).count());
        }

        // Builds a fresh stamp for a source file
        inline AssetStamp Build(const String& sourceAbsPath, Uint cookerVersion)
        {
            AssetStamp stamp;
            stamp.Magic = AssetStamp::kMagic;
            stamp.CookerVersion = cookerVersion;
            stamp.SourceModTime = GetModTime(sourceAbsPath);
            return stamp;
        }

        // Read only the stamp from a binary asset, returns false if the stamp is bad or the file can't be read
        inline bool Read(const String& sidecarPath, AssetStamp& outStamp)
        {
            Vector<Byte> buffer;
            if(!Filesystem::ReadBinaryFilePartial(sidecarPath, buffer, sizeof(AssetStamp)))
                return false;
            if(buffer.size() < sizeof(AssetStamp))
                return false;

            memcpy(&outStamp, buffer.data(), sizeof(AssetStamp));
            return outStamp.Magic == AssetStamp::kMagic;
        }

        inline bool IsUpToDate(const AssetStamp& stamp, const String& sourceAbsPath, Uint currentCookerVersion)
        {
            if(stamp.CookerVersion != currentCookerVersion)
                return false;
            if(stamp.SourceModTime != GetModTime(sourceAbsPath))
                return false;

            return true;
        }
    }
}