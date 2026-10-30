// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Asset/Asset.hpp"
#include "Surge/Core/Path.hpp"
#include "Surge/Core/Vector.hpp"
#include "Surge/Core/Raptor/Reader.hpp"
#include <span>

namespace Surge
{
    // Starts every cooked file (Internal/<TYPE>/<id>.r<TYPE>, Internal/Registry.rREG, Internal/Project.rPROJ, EngineUI.sFONT), the payload follows.
    // The engine checks Magic, Type, FormatVersion and PayloadSize before a format reads the payload. CookerVersion, SourceModTime and
    // InputHash are only read by the Editor, to know whether the file is still up to date with its source
    struct CookedHeader
    {
        static constexpr Uint MAGIC = 0x43475253; // SRGC

        Uint Magic = MAGIC;
        AssetType Type = AssetType::NONE;
        uint8_t Flags = 0;           // Reserved (payload compression)
        uint16_t FormatVersion = 0;  // The format's VERSION, a mismatch fails the load
        Uint CookerVersion = 0;
        Uint PayloadSize = 0;
        uint64_t SourceModTime = 0;  // Newest write time of the inputs (seconds)
        uint64_t InputHash = 0;      // FNV-1a of the inputs (source, import settings, cooker extras)
    };
    static_assert(sizeof(CookedHeader) == 32 && std::is_trivially_copyable_v<CookedHeader>, "CookedHeader is read and written as raw bytes");

    enum class CookedFileError : uint8_t
    {
        NONE = 0,
        UNREADABLE,  // Missing or not readable
        NOT_COOKED,  // No CookedHeader (an older cooked format, or not a cooked file at all)
        WRONG_TYPE,  // Cooked for another asset type
        OLD_FORMAT,  // Cooked by another version of the format
        TRUNCATED    // Shorter than its header says
    };
    const char* CookedFileErrorToString(CookedFileError error);

    // A cooked file read into memory, its header checked
    struct CookedFile
    {
        CookedHeader Header;
        Vector<Byte> Bytes; // The whole file

        Rapt0R::Reader GetPayload() const { return Rapt0R::Reader(Bytes.data() + sizeof(CookedHeader), Header.PayloadSize); }

        // The payload bytes without a copy (drops the header in place), for formats that keep their payload as is (audio, scripts)
        Vector<Byte> TakePayload();
    };

    namespace CookedFileIO
    {
        // Reads and checks a whole cooked file
        CookedFileError Read(const Path& path, AssetType type, uint16_t formatVersion, CookedFile& outFile);

        // Reads only the header (Editor freshness checks), false when there is none
        bool ReadHeader(const Path& path, CookedHeader& outHeader);

        // header + payload. Sets the header's Magic, FormatVersion and PayloadSize
        bool Write(const Path& path, CookedHeader header, uint16_t formatVersion, std::span<const Byte> payload);
    }

} // namespace Surge
