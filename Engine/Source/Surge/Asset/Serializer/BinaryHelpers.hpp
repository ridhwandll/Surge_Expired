// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Defines.hpp"

namespace Surge
{
    // Writers
    template <typename T>
    static void WriteData(Vector<Byte>& buffer, const T& data)
    {
        const Byte* ptr = reinterpret_cast<const Byte*>(&data);
        buffer.insert(buffer.end(), ptr, ptr + sizeof(T));
    }

    template <typename T>
    static void WriteDataArray(Vector<Byte>& buffer, const T* data, size_t count)
    {
        if(count == 0)
            return;
        const Byte* ptr = reinterpret_cast<const Byte*>(data);
        buffer.insert(buffer.end(), ptr, ptr + (sizeof(T) * count));
    }

    static void WriteStr(Vector<Byte>& buffer, const String& s)
    {
        Uint len = static_cast<Uint>(s.size());
        WriteData(buffer, len);
        WriteDataArray(buffer, s.data(), s.size());
    }

    // Readers
    template <typename T>
    static void ReadData(const Byte*& ptr, T& data)
    {
        memcpy(&data, ptr, sizeof(T));
        ptr += sizeof(T);
    }

    template <typename T>
    static void ReadDataArray(const Byte*& ptr, T* data, size_t count)
    {
        if(count == 0)
            return;
        memcpy(data, ptr, sizeof(T) * count);
        ptr += sizeof(T) * count;
    }

    static String ReadStr(const Byte*& ptr)
    {
        Uint len = 0;
        ReadData(ptr, len);
        String s(len, '\0');
        if(len > 0)
            ReadDataArray(ptr, s.data(), len);
        return s;
    }
}
