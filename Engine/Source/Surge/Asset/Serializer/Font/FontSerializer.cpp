// Copyright (c) - SurgeTechnologies - All rights reserved
#include "FontSerializer.hpp"
#include "Surge/Core/Core.hpp"
#include "FontBinaryFormat.hpp"
#include "Surge/Asset/AssetManager.hpp"

namespace Surge
{
    void FontSerializer::Initialize()
    {
        mSerializerType = AssetType::FONT;
    }

    bool FontSerializer::Serialize([[maybe_unused]] Ref<Asset> asset) const
    {
#ifdef SURGE_PLATFORM_ANDROID
        Log<Severity::Error>("[FontSerializer] Serialization is unsupported on Android runtime. Pre-cook the assets!");
        return false;
#else
        AssetManager* am = Core::GetAssetManager();
        const AssetMetadata& meta = am->GetMetadata(asset->GetID());
        if(HasFlag(meta.Flags, AssetFlags::MEMORY))
            return true;

        SCOPED_TIMER("FontSerializer::Serialize");
        Ref<Font> font = asset.As<Font>();
        const String sidecarPath = am->GetSidecarPath(meta.ID);

        AssetStamp existingStamp;
        [[maybe_unused]] bool stampRead = AssetStampWriter::Read(sidecarPath, existingStamp);
        SG_ASSERT(stampRead, "Failed to read asset stamp");

        bool result = FontBinary::Write(sidecarPath, existingStamp, font->GetSpecification());
        return result;
#endif
    }

    Ref<Asset> FontSerializer::Deserialize(const AssetMetadata& metadata) const
    {
        AssetManager* am = Core::GetAssetManager();
        const String sidecarPath = am->GetSidecarPath(metadata.ID);

        SCOPED_TIMER("FontSerializer::Deserialize: {}", sidecarPath);
        AssetStamp existingStamp;
        FontSpecification spec;
        if(!FontBinary::Read(sidecarPath, existingStamp, spec))
        {
            Log<Severity::Error>("[FontSerializer] Failed to read sidecar for font {}, returing nullptr!", metadata.RelativePath);
            return nullptr;
        }

        return Font::Create(std::move(spec)).As<Asset>();
    }

    void FontSerializer::Shutdown()
    {
        Log<Severity::Info>("[FontSerializer] Shutdown");
    }
}