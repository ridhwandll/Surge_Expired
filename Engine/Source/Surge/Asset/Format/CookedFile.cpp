// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Surge/Asset/Format/CookedFile.hpp"
#include "Surge/Asset/Format/BinarySerialization.hpp"
#include "Surge/Utility/Filesystem.hpp"

namespace Surge
{
    SURGE_RAPTOR_RAW(CookedHeader, 32);

    const char* CookedFileErrorToString(CookedFileError error)
    {
        switch(error)
        {
            case CookedFileError::NONE: return "no error";
            case CookedFileError::UNREADABLE: return "the file is missing or can't be read";
            case CookedFileError::NOT_COOKED: return "it isn't a cooked file (or was cooked by an older Editor)";
            case CookedFileError::WRONG_TYPE: return "it was cooked for another asset type";
            case CookedFileError::OLD_FORMAT: return "it was cooked with another format version";
            case CookedFileError::TRUNCATED: return "it is truncated";
        }
        return "unknown error";
    }

    Vector<Byte> CookedFile::TakePayload()
    {
        Vector<Byte> payload = std::move(Bytes);
        payload.erase(payload.begin(), payload.begin() + sizeof(CookedHeader));
        payload.resize(Header.PayloadSize);
        return payload;
    }

    namespace CookedFileIO
    {
        CookedFileError Read(const Path& path, AssetType type, uint16_t formatVersion, CookedFile& outFile)
        {
            if(!Filesystem::ReadBinaryFile(path, outFile.Bytes))
                return CookedFileError::UNREADABLE;

            Rapt0R::Reader reader(outFile.Bytes.data(), outFile.Bytes.size());
            if(!reader.Read(outFile.Header) || outFile.Header.Magic != CookedHeader::MAGIC)
                return CookedFileError::NOT_COOKED;
            if(outFile.Header.Type != type)
                return CookedFileError::WRONG_TYPE;
            if(outFile.Header.FormatVersion != formatVersion)
                return CookedFileError::OLD_FORMAT;
            if(reader.RemainingBytes() < outFile.Header.PayloadSize)
                return CookedFileError::TRUNCATED;
            return CookedFileError::NONE;
        }

        bool ReadHeader(const Path& path, CookedHeader& outHeader)
        {
            Vector<Byte> bytes;
            if(!Filesystem::ReadBinaryFilePartial(path, bytes, sizeof(CookedHeader)))
                return false;

            Rapt0R::Reader reader(bytes.data(), bytes.size());
            return reader.Read(outHeader) && outHeader.Magic == CookedHeader::MAGIC;
        }

        bool Write(const Path& path, CookedHeader header, uint16_t formatVersion, std::span<const Byte> payload)
        {
            Rapt0R::Writer writer(sizeof(CookedHeader) + payload.size());
            if(!writer.ValidateCount(payload.size()))
                return false;

            header.Magic = CookedHeader::MAGIC;
            header.FormatVersion = formatVersion;
            header.PayloadSize = static_cast<Uint>(payload.size());
            writer.Write(header);
            writer.WriteRawBytes(payload.data(), payload.size());

            Filesystem::CreateOrEnsureDirectories(path.parent_path());
            return !writer.HasError() && Filesystem::WriteBinaryFile(path, writer.Data(), writer.Size());
        }
    }

} // namespace Surge
