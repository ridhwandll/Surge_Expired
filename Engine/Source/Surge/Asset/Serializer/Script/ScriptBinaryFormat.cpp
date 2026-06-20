// Copyright (c) - SurgeTechnologies - All rights reserved
#include "ScriptBinaryFormat.hpp"
#include "Surge/Asset/Serializer/BinaryHelpers.hpp"
#include "Surge/Utility/Filesystem.hpp"
#include "Surge/ScriptEngine/ScriptAsset.hpp"

namespace Surge::ScriptBinary
{
    bool Write(const String& path, const AssetStamp& stamp, const Ref<Script>& script)
    {
        const Vector<Byte>& bytecode = script->GetBytecode();

        Vector<Byte> stampBuffer;
        WriteData(stampBuffer, stamp);

        Vector<Byte> outBuffer;
        WriteData(outBuffer, static_cast<uint64_t>(bytecode.size()));
        outBuffer.insert(outBuffer.end(), bytecode.begin(), bytecode.end());

        // Prepend the stamp to the output buffer
        outBuffer.insert(outBuffer.begin(), stampBuffer.begin(), stampBuffer.end());

        if(!Filesystem::WriteBinaryFile(path, outBuffer.data(), outBuffer.size()))
        {
            Log<Severity::Error>("[ScriptBinary] Failed to write sidecar: {}", path);
            return false;
        }
        Log<Severity::Trace>("[ScriptBinary] Cooked sidecar: {}", path);
        return true;
    }

    bool Read(const String& path, AssetStamp& outStamp, Ref<Script>& outScript)
    {
        Vector<Byte> scriptCodeBuffer;
        if(Filesystem::ReadBinaryFile(path, scriptCodeBuffer))
        {
            if(scriptCodeBuffer.size() < sizeof(AssetStamp) + sizeof(uint64_t))
            {
                Log<Severity::Error>("[ScriptSerializer] Corrupted script sidecar (Too small): {}", path);
                return false;
            }

            const Byte* ptr = scriptCodeBuffer.data();
            const Byte* endPtr = scriptCodeBuffer.data() + scriptCodeBuffer.size();

            ReadData(ptr, outStamp);

            uint64_t bytecodeSize = 0;
            ReadData(ptr, bytecodeSize);

            // ptr is now at the start of the bytecode

            if(ptr + bytecodeSize > endPtr)
            {
                Log<Severity::Error>("[ScriptSerializer] Corrupted script sidecar (Bytecode size mismatch): {}", path);
                return false;
            }

            Vector<Byte> bytecode(ptr, ptr + bytecodeSize);
            outScript = Script::Create(std::move(bytecode));
            return true;
        }
        else
        {
            Log<Severity::Error>("[ScriptBinary] Failed to read sidecar at path: {}", path);
            return false;
        }
    }
}
