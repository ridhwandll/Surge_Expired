// Copyright (c) - SurgeTechnologies - All rights reserved
#include "MaterialBinaryFormat.hpp"
#include "Surge/Asset/Serializer/BinaryHelpers.hpp"
#include "Surge/Core/Core.hpp"
#include "Surge/Utility/Filesystem.hpp"

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

    Ref<Material> ReadBuffer(const Byte*& ptr)
    {
        Byte hasData = 0;
        ReadData(ptr, hasData);
        if(!hasData)
            return nullptr;

        const String name = ReadStr(ptr);

        Uint propsSize = 0;
        ReadData(ptr, propsSize);

        // Create with hardcoded pipeline/shader allocates CPU buffer via shader reflection
        Ref<Material> mat = Material::Create(name);
        MemoryBlock& cpuBuffer = mat->GetCPUData();
        SG_ASSERT(propsSize == static_cast<Uint>(cpuBuffer.GetSize()), "[MeshSerializer] Inline material {} props size mismatch ({} vs {}). Shader UBO layout changed, delete sidecar and reimport", name, propsSize, static_cast<Uint>(cpuBuffer.GetSize()));

        ReadDataArray(ptr, cpuBuffer.As<Byte>(), propsSize);

        Uint texCount = 0;
        ReadData(ptr, texCount);
        AssetManager* am = Core::GetAssetManager();
        for(Uint i = 0; i < texCount; i++)
        {
            const String texName = ReadStr(ptr);
            uint64_t rawID = 0;
            ReadData(ptr, rawID);

            Ref<Texture2D> tex = am->Load<Texture2D>(AssetID(rawID));
            if(tex)
                mat->SetTexture(texName, tex);
            else
                Log<Severity::Warn>("[MeshSerializer] Material {}: texture ID {} failed to load!", name, rawID);
        }

        mat->MarkDirty();
        return mat;
    }

    bool Write(const String& path, const Ref<Material>& mat)
    {
        Vector<Byte> outBuffer;
        WriteBuffer(outBuffer, mat);

        if(!Filesystem::WriteBinaryFile(path, outBuffer.data(), outBuffer.size()))
        {
            Log<Severity::Error>("[MaterialBinary] Failed to write sidecar: {}", path);
            return false;
        }
        Log<Severity::Trace>("[MaterialBinary] Cooked sidecar: {}", path);
        return true;
    }

    bool Read(const String& path, Ref<Material>& outMat)
    {
        Vector<Byte> fileData;
        if(!Filesystem::ReadBinaryFile(path, fileData))
            return false;

        const Byte* ptr = fileData.data();
        outMat = ReadBuffer(ptr);
        return true;
    }
}
