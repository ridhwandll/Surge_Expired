// Copyright (c) - SurgeTechnologies - All rights reserved
#include "AudioCooker.hpp"
#include "Surge/Asset/Format/BinarySerialization.hpp"
#include "Surge/Asset/Format/Audio/AudioFormat.hpp"
#include "Surge/Utility/Filesystem.hpp"

namespace Surge
{
    bool AudioCooker::Cook(const CookContext& context) const
    {
        // The encoded file is kept as is, the AudioEngine decodes/streams it at runtime
        Vector<Byte> source;
        if(!Filesystem::ReadBinaryFile(context.SourcePath, source) || source.empty())
        {
            Log<Severity::Error>("[AudioCooker] Failed to read {}", context.SourcePath.string());
            return false;
        }
        Rapt0R::Writer payload(source.size());
        AudioBinary::Write(payload, source);
        return context.Write(AudioBinary::VERSION, payload);
    }
}
