// Copyright (c) - SurgeTechnologies - All rights reserved
#include "MaterialBinaryFormat.hpp"
#include "Surge/Asset/Serializer/BinaryHelpers.hpp"
#include "Surge/Utility/Filesystem.hpp"
#include "Surge/Asset/AssetManager.hpp"

namespace Surge::MaterialBinary
{
    Uint CalculateSize(const Ref<Material>& mat)
    {
        Uint size = 1; // hasData boolean flag (1 byte)
        if(!mat)
            return size;

        // Material Name (Uint length + string chars)
        size += sizeof(Uint) + static_cast<Uint>(mat->GetName().size());

        // UBO / CPU Data (Uint length + buffer bytes)
        size += sizeof(Uint) + static_cast<Uint>(mat->GetCPUData().GetSize());

        // Textures
        size += sizeof(Uint); // texCount
        for(const auto& [name, tex] : mat->GetTextures())
        {
            if(tex.Data1 && tex.Data1->GetID().IsValid())
            {
                size += sizeof(Uint) + static_cast<Uint>(name.size()); // String length + chars
                size += sizeof(uint64_t); // rawID
            }
        }
        return size;
    }

    void WriteBuffer(Vector<Byte>& buffer, const Ref<Material>& mat)
    {
        const Byte hasData = mat ? 1 : 0;
        WriteData(buffer, hasData);

        if(!hasData)
            return;

        WriteStr(buffer, mat->GetName());

        // CPU buffer
        const MemoryBlock& cpuBuffer = mat->GetCPUData();
        const Uint propsSize = static_cast<Uint>(cpuBuffer.GetSize());
        WriteData(buffer, propsSize);
        WriteDataArray(buffer, cpuBuffer.As<Byte>(), propsSize);

        // Only owned Ref<Texture2D> slots with a valid AssetID
        const auto& textures = mat->GetTextures();
        Uint texCount = 0;
        for(const auto& [name, tex] : textures)
        {
            if(tex.Data1 && tex.Data1->GetID().IsValid())
                texCount++;
        }

        WriteData(buffer, texCount);
        for(const auto& [name, tex] : textures)
        {
            if(!tex.Data1 || !tex.Data1->GetID().IsValid())
                continue;

            WriteStr(buffer, name); // Ex: AlbedoMap, NormalMap (Name in shader)
            const uint64_t rawID = tex.Data1->GetID().Get();
            WriteData(buffer, rawID);
        }
    }

    Ref<Material> ReadBuffer(const AssetContext& context, BinaryReader& reader)
    {
        Byte hasData = 0;
        reader.Read(hasData);
        if(!hasData)
            return nullptr;

        const String name = reader.ReadStr();

        Uint propsSize = 0;
        reader.Read(propsSize);
        if(reader.Failed)
            return nullptr;

        // Create with hardcoded pipeline/shader allocates CPU buffer via shader reflection
        Ref<Material> mat = Material::Create(*context.Renderer_, name);
        MemoryBlock& cpuBuffer = mat->GetCPUData();
        if(propsSize == static_cast<Uint>(cpuBuffer.GetSize()))
        {
            reader.ReadArray(cpuBuffer.As<Byte>(), propsSize);
        }
        else
        {
            // Stale cooked data (the shader UBO layout changed), the Player never re-cooks so keep the defaults instead of overflowing the block
            Log<Severity::Error>("[MaterialBinary] Material {}: property size mismatch ({} vs {}), the shader UBO layout changed. Re-cook it, using defaults", name, propsSize, static_cast<Uint>(cpuBuffer.GetSize()));
            reader.Skip(propsSize);
        }

        Uint texCount = 0;
        reader.Read(texCount);
        AssetManager* am = context.AssetManager_;
        for(Uint i = 0; i < texCount; i++)
        {
            const String texName = reader.ReadStr();
            uint64_t rawID = 0;
            reader.Read(rawID);
            if(reader.Failed)
                break;

            Ref<Texture2D> tex = am->Load<Texture2D>(AssetID(rawID));
            if(tex)
                mat->SetTexture(texName, tex);
            else
                Log<Severity::Warn>("[MaterialBinary] Material {}: texture ID {} failed to load!", name, rawID);
        }

        if(reader.Failed)
            return nullptr;

        mat->MarkDirty();
        return mat;
    }

    bool Write(const String& path, const AssetStamp& stamp, const Ref<Material>& mat)
    {
        Vector<Byte> stampBuffer;
        WriteData(stampBuffer, stamp);

        Vector<Byte> outBuffer;
        WriteBuffer(outBuffer, mat);

        // Prepend the stamp to the output buffer
        outBuffer.insert(outBuffer.begin(), stampBuffer.begin(), stampBuffer.end());

        if(!Filesystem::WriteBinaryFile(path, outBuffer.data(), outBuffer.size()))
        {
            Log<Severity::Error>("[MaterialBinary] Failed to write sidecar: {}", path);
            return false;
        }
        Log<Severity::Trace>("[MaterialBinary] Cooked sidecar: {}", path);
        return true;
    }

    bool Read(const AssetContext& context, const String& path, AssetStamp& outStamp, Ref<Material>& outMat)
    {
        Vector<Byte> fileData;
        if(!Filesystem::ReadBinaryFile(path, fileData))
            return false;

        BinaryReader reader(fileData.data(), fileData.size());
        reader.Read(outStamp);
        outMat = ReadBuffer(context, reader);
        if(reader.Failed)
        {
            Log<Severity::Error>("[MaterialBinary] {} is truncated or corrupted!", path);
            outMat = nullptr;
            return false;
        }
        return true;
    }
}
