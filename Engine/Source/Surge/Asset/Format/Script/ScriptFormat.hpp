// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Asset/Format/AssetFormat.hpp"
#include <span>

namespace Rapt0R
{
    class Reader;
}

namespace Surge
{
    // ScriptBinary (.rSCRIPT) payload: the Lua bytecode compiled by the ScriptCooker, as is
    namespace ScriptBinary
    {
        inline constexpr uint16_t VERSION = 1;

        void Write(Rapt0R::Writer& writer, std::span<const Byte> bytecode);
        bool Read(Rapt0R::Reader& reader, std::span<const Byte>& outBytecode); // A view of the payload, false when it is empty
    }

    class ScriptFormat final : public AssetFormat
    {
    public:
        virtual AssetType GetType() const override { return AssetType::SCRIPT; }
        virtual uint16_t GetFormatVersion() const override { return ScriptBinary::VERSION; }
        virtual Ref<Asset> Read(CookedFile& file, const AssetMetadata& metadata, const AssetContext& context) const override;
        virtual bool Write(Rapt0R::Writer& payload, const Asset& asset) const override;
    };

} // namespace Surge
