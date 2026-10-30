// Copyright (c) - SurgeTechnologies - All rights reserved
#include "AudioFormat.hpp"
#include "Surge/Asset/Format/BinarySerialization.hpp"
#include "Surge/Audio/Audio.hpp"

namespace Surge
{
    namespace AudioBinary
    {
        void Write(Rapt0R::Writer& writer, std::span<const Byte> encodedData)
        {
            writer.WriteRawBytes(encodedData.data(), encodedData.size());
        }

        bool Read(Rapt0R::Reader& reader, std::span<const Byte>& outEncodedData)
        {
            outEncodedData = reader.UnreadSpan();
            return reader.Skip(outEncodedData.size()) && !outEncodedData.empty();
        }
    }

    Ref<Asset> AudioFormat::Read(CookedFile& file, const AssetMetadata& metadata, [[maybe_unused]] const AssetContext& context) const
    {
        std::span<const Byte> encodedData;
        Rapt0R::Reader payload = file.GetPayload();
        if(!AudioBinary::Read(payload, encodedData))
        {
            Log<Severity::Error>("[AudioFormat] {} has no audio data", metadata.RelativePath);
            return nullptr;
        }

        // The whole payload is the clip: the asset takes the file's allocation instead of copying the view
        return Ref<Audio>::Create(file.TakePayload()).As<Asset>();
    }

    bool AudioFormat::Write(Rapt0R::Writer& payload, const Asset& asset) const
    {
        AudioBinary::Write(payload, static_cast<const Audio&>(asset).GetEncodedData());
        return !payload.HasError();
    }

} // namespace Surge
