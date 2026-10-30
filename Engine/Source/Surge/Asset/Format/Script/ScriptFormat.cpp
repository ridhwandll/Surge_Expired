// Copyright (c) - SurgeTechnologies - All rights reserved
#include "ScriptFormat.hpp"
#include "Surge/Asset/Format/BinarySerialization.hpp"
#include "Surge/ScriptEngine/ScriptAsset.hpp"

namespace Surge
{
    namespace ScriptBinary
    {
        void Write(Rapt0R::Writer& writer, std::span<const Byte> bytecode)
        {
            writer.WriteRawBytes(bytecode.data(), bytecode.size());
        }

        bool Read(Rapt0R::Reader& reader, std::span<const Byte>& outBytecode)
        {
            outBytecode = reader.UnreadSpan();
            return reader.Skip(outBytecode.size()) && !outBytecode.empty();
        }
    }

    Ref<Asset> ScriptFormat::Read(CookedFile& file, const AssetMetadata& metadata, [[maybe_unused]] const AssetContext& context) const
    {
        std::span<const Byte> bytecode;
        Rapt0R::Reader payload = file.GetPayload();
        if(!ScriptBinary::Read(payload, bytecode))
        {
            Log<Severity::Error>("[ScriptFormat] {} has no bytecode", metadata.RelativePath);
            return nullptr;
        }

        // The whole payload is the bytecode: the script takes the file's allocation instead of copying the view
        return Script::Create(file.TakePayload()).As<Asset>();
    }

    bool ScriptFormat::Write(Rapt0R::Writer& payload, const Asset& asset) const
    {
        ScriptBinary::Write(payload, static_cast<const Script&>(asset).GetBytecode());
        return !payload.HasError();
    }

} // namespace Surge
