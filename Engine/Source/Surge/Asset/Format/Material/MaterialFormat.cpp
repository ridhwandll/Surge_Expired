// Copyright (c) - SurgeTechnologies - All rights reserved
#include "MaterialFormat.hpp"
#include "Surge/Asset/Format/BinarySerialization.hpp"
#include "Surge/Asset/AssetManager.hpp"

namespace Surge
{
    namespace MaterialBinary
    {
        // Only owned Ref<Texture2D> slots with a valid AssetID are saved
        static bool IsSavedTexture(const auto& slot)
        {
            return slot.Data1 && slot.Data1->GetID().IsValid();
        }

        void Write(Rapt0R::Writer& writer, const Material* mat)
        {
            writer.Write(mat != nullptr);
            if(!mat)
                return;

            const MemoryBlock& cpuBuffer = mat->GetCPUData();
            const auto& textures = mat->GetTextures();
            Uint texCount = 0;
            for(const auto& [name, tex] : textures)
                texCount += IsSavedTexture(tex) ? 1 : 0;

            writer.Write(mat->GetName());
            writer.WriteSpan(std::span<const Byte>(cpuBuffer.As<Byte>(), cpuBuffer.GetSize()));
            writer.Write(texCount);
            for(const auto& [name, tex] : textures)
            {
                if(IsSavedTexture(tex))
                    writer.Write(name, tex.Data1->GetID().Get());
            }
        }

        Ref<Material> Read(const AssetContext& context, Rapt0R::Reader& reader)
        {
            bool hasData = false;
            String name;
            std::span<const Byte> props;
            if(!reader.Read(hasData) || !hasData || !reader.Read(name) || !reader.ReadSpan(props))
                return nullptr;

            // Create with hardcoded pipeline/shader allocates CPU buffer via shader reflection
            Ref<Material> mat = Material::Create(*context.Renderer_, name);
            MemoryBlock& cpuBuffer = mat->GetCPUData();
            if(props.size() == cpuBuffer.GetSize())
            {
                if(!props.empty())
                    std::memcpy(cpuBuffer.As<Byte>(), props.data(), props.size());
            }
            else
            {
                // Stale cooked data (the shader UBO layout changed), the Player never re-cooks so keep the defaults instead of overflowing the block
                Log<Severity::Error>("[MaterialBinary] Material {}: property size mismatch ({} vs {}), the shader UBO layout changed. Re-cook it, using defaults", name, props.size(), cpuBuffer.GetSize());
            }

            Uint texCount = 0;
            if(!reader.Read(texCount) || !reader.EnsureElements(texCount, sizeof(Uint) + sizeof(uint64_t)))
                return nullptr;

            AssetManager* am = context.AssetManager_;
            for(Uint i = 0; i < texCount; i++)
            {
                String texName;
                uint64_t rawID = 0;
                if(!reader.Read(texName, rawID))
                    return nullptr;

                Ref<Texture2D> tex = am->Load<Texture2D>(AssetID(rawID));
                if(tex)
                    mat->SetTexture(texName, tex);
                else
                    Log<Severity::Warn>("[MaterialBinary] Material {}: texture ID {} failed to load!", name, rawID);
            }

            mat->MarkDirty();
            return mat;
        }
    }

    Ref<Asset> MaterialFormat::Read(CookedFile& file, const AssetMetadata& metadata, const AssetContext& context) const
    {
        Rapt0R::Reader payload = file.GetPayload();
        Ref<Material> material = MaterialBinary::Read(context, payload);
        if(!material)
        {
            Log<Severity::Error>("[MaterialFormat] {} is empty, truncated or corrupted!", metadata.RelativePath);
            return nullptr;
        }
        return material.As<Asset>();
    }

    bool MaterialFormat::Write(Rapt0R::Writer& payload, const Asset& asset) const
    {
        MaterialBinary::Write(payload, &static_cast<const Material&>(asset));
        return !payload.HasError();
    }

} // namespace Surge
