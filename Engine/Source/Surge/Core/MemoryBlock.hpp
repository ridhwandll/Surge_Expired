// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Defines.hpp"

namespace Surge
{
    struct MemoryBlock
    {
        void* Data;
        Uint Size;

        MemoryBlock()
            : Data(nullptr), Size(0) {}

        MemoryBlock(std::nullptr_t)
            : Data(nullptr), Size(0) {}

        MemoryBlock(void* data, Uint size)
            : Data(data), Size(size) {}

        ~MemoryBlock()
        {
            Release();
        }

        SURGE_DISABLE_COPY(MemoryBlock);

        MemoryBlock(MemoryBlock&& other) noexcept
            : Data(other.Data), Size(other.Size)
        {
            other.Data = nullptr;
            other.Size = 0;
        }

        MemoryBlock& operator=(MemoryBlock&& other) noexcept
        {
            if(this != &other)
            {
                Release();
                Data = other.Data;
                Size = other.Size;
                other.Data = nullptr;
                other.Size = 0;
            }
            return *this;
        }


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
        const T& Read(Uint offset = 0) const
        {
            return *(T*)((Byte*)Data + offset);
        }

        template <typename T>
        T& Read(Uint offset = 0)
        {
            return *(T*)((Byte*)Data + offset);
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
        const T* As() const
        {
            return (T*)Data;
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