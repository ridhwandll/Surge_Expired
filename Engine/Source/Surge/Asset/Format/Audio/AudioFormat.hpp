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
    // AudioBinary (.rAUDIO) payload: the source file (wav/mp3/ogg/flac) byte for byte. The Audio asset keeps it, the AudioEngine plays it from memory
    namespace AudioBinary
    {
        inline constexpr uint16_t VERSION = 1;

        void Write(Rapt0R::Writer& writer, std::span<const Byte> encodedData);
        bool Read(Rapt0R::Reader& reader, std::span<const Byte>& outEncodedData); // A view of the payload, false when it is empty
    }

    class AudioFormat final : public AssetFormat
    {
    public:
        virtual AssetType GetType() const override { return AssetType::AUDIO; }
        virtual uint16_t GetFormatVersion() const override { return AudioBinary::VERSION; }
        virtual Ref<Asset> Read(CookedFile& file, const AssetMetadata& metadata, const AssetContext& context) const override;
        virtual bool Write(Rapt0R::Writer& payload, const Asset& asset) const override;
    };

} // namespace Surge
