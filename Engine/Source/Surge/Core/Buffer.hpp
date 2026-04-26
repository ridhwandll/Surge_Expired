// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Defines.hpp"

namespace Surge
{
    struct Buffer
    {
        void* Data;
        Uint Size;

        Buffer()
            : Data(nullptr), Size(0) {}

        Buffer(void* data, Uint size)
            : Data(data), Size(size) {}

        void Allocate(Uint size)
        {
            delete[] static_cast<Byte*>(Data);
            Data = nullptr;

            if (size == 0)
                return;

            Data = new Byte[size];
            Size = size;
        }

        void Release()
        {
            delete[] static_cast<Byte*>(Data);
            Data = nullptr;
            Size = 0;
        }

        void ZeroInitialize()
        {
            if (Data)
                memset(Data, 0, Size);
        }

        template <typename T>
        T& Read(Uint offset = 0)
        {
            return *(T*)((Byte*)Data + offset);
        }

        Byte* ReadBytes(Uint size, Uint offset)
        {
            SG_ASSERT(offset + size <= Size, "Buffer overflow!");
            Byte* buffer = new Byte[size];
            std::memcpy(buffer, (Byte*)Data + offset, size);
            return buffer;
        }

        void Write(void* data, Uint size, Uint offset = 0)
        {
            SG_ASSERT(offset + size <= Size, "Buffer overflow!");
            std::memcpy((Byte*)Data + offset, data, size);
        }

        operator bool() const
        {
            return Data;
        }

        Byte& operator[](int index)
        {
            return ((Byte*)Data)[index];
        }

        Byte operator[](int index) const
        {
            return ((Byte*)Data)[index];
        }

        template <typename T>
        T* As()
        {
            return (T*)Data;
        }

        Uint GetSize() const
        {
            return Size;
        }
    };
} // namespace Surge