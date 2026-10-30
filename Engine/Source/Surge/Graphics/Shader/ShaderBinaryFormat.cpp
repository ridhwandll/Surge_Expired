// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Surge/Graphics/Shader/ShaderBinaryFormat.hpp"
#include "Surge/Asset/Format/BinarySerialization.hpp"
#include "Surge/Utility/Filesystem.hpp"

namespace Surge
{
    SURGE_RAPTOR_FIELDS(SPIRVHandle, v.Type, v.SPIRV)
}

namespace Surge::ShaderBinary
{
    static constexpr Uint SPIRV_MAGIC = 0x07230203;
    static constexpr Uint MAX_INCLUDES = 256;

    static void HashBytes(uint64_t& hash, const Byte* data, size_t size) // FNV-1a, '\r' skipped so CRLF checkouts hash like LF ones
    {
        for (size_t i = 0; i < size; i++)
        {
            if (data[i] == '\r')
                continue;
            hash ^= data[i];
            hash *= 1099511628211ull;
        }
    }

    // Everything up to the stages: what the Editor needs to know whether the cooked file is outdated
    static bool ReadPrefix(Rapt0R::Reader& reader, uint64_t& outHash, ShaderType& outStages, Vector<String>& outIncludes)
    {
        Uint magic = 0;
        Uint version = 0;
        if (!reader.Read(magic, version) || magic != MAGIC || version != VERSION)
            return false;
        return reader.Read(outHash, outStages, outIncludes) && outIncludes.size() <= MAX_INCLUDES;
    }

    String GetCookedPath(const String& sourcePath)
    {
        return std::format("{0}/Cooked/{1}.rSHADER", Filesystem::GetParentPath(sourcePath).string(), Filesystem::GetFilenameWithoutExt(sourcePath));
    }

    uint64_t HashSource(const String& sourcePath, const Vector<String>& includes)
    {
        uint64_t hash = 14695981039346656037ull;
        Vector<Byte> content;
        if (!Filesystem::ReadBinaryFile(sourcePath, content))
            return 0;
        HashBytes(hash, content.data(), content.size());

        const String baseDirectory = Filesystem::GetParentPath(sourcePath).string();
        for (const String& include : includes)
        {
            HashBytes(hash, reinterpret_cast<const Byte*>(include.data()), include.size());
            if (!Filesystem::ReadBinaryFile(std::format("{0}/{1}", baseDirectory, include), content))
                return 0;
            HashBytes(hash, content.data(), content.size());
        }
        return hash == 0 ? 1 : hash; // 0 means unreadable
    }

    bool Write(const String& path, const CookedShader& shader)
    {
        size_t spirvBytes = 0;
        for (const SPIRVHandle& spirv : shader.SPIRVs)
            spirvBytes += spirv.SPIRV.size() * sizeof(Uint);

        Rapt0R::Writer writer(spirvBytes + 8192);
        writer.Write(MAGIC, VERSION, shader.SourceHash, shader.Stages, shader.Includes, shader.SPIRVs, shader.Reflection);

        Filesystem::CreateOrEnsureDirectories(Filesystem::GetParentPath(path));
        if (writer.HasError() || !Filesystem::WriteBinaryFile(path, writer.Data(), writer.Size()))
        {
            Log<Severity::Error>("[ShaderBinary] Failed to write: {}", path);
            return false;
        }
        return true;
    }

    bool Read(const String& path, CookedShader& outShader)
    {
        outShader = CookedShader();

        Vector<Byte> buffer;
        if (!Filesystem::ReadBinaryFile(path, buffer))
        {
            Log<Severity::Error>("[ShaderBinary] Failed to read: {}", path);
            return false;
        }

        Rapt0R::Reader reader(buffer.data(), buffer.size());
        if (!ReadPrefix(reader, outShader.SourceHash, outShader.Stages, outShader.Includes))
        {
            Log<Severity::Error>("[ShaderBinary] Outdated or corrupted cooked shader (expected version {}): {}", VERSION, path);
            return false;
        }

        if (!reader.Read(outShader.SPIRVs))
        {
            Log<Severity::Error>("[ShaderBinary] Corrupted cooked shader (SPIR-V size): {}", path);
            return false;
        }

        ShaderType stagesFound = ShaderType::NONE;
        for (const SPIRVHandle& spirv : outShader.SPIRVs)
        {
            if (spirv.SPIRV.empty() || spirv.SPIRV[0] != SPIRV_MAGIC || (static_cast<Uint>(spirv.Type) & static_cast<Uint>(outShader.Stages)) == 0)
            {
                Log<Severity::Error>("[ShaderBinary] Corrupted cooked shader (invalid stage): {}", path);
                return false;
            }
            stagesFound |= spirv.Type;
        }

        if (stagesFound != outShader.Stages || !reader.Read(outShader.Reflection))
        {
            Log<Severity::Error>("[ShaderBinary] Corrupted cooked shader (reflection): {}", path);
            return false;
        }
        return true;
    }

    bool ReadSourceInfo(const String& path, uint64_t& outHash, Vector<String>& outIncludes)
    {
        Vector<Byte> buffer;
        if (!Filesystem::ReadBinaryFile(path, buffer))
            return false;

        Rapt0R::Reader reader(buffer.data(), buffer.size());
        ShaderType stages = ShaderType::NONE;
        return ReadPrefix(reader, outHash, stages, outIncludes);
    }
}
