/*
  +------------------------------------------------+
  |  _____             _    ___  _____    01010010 |
  | │  __ ╲           │ │  ╱ _ ╲│  __ ╲   01100001 |
  | │ │__) │__ _ _ __ │ │_│ │ │ │ │__) │  01110000 |
  | │  _  ╱╱ _` │ '_ ╲│ __│ │ │ │  _  ╱   01110100 |
  | │ │ ╲ ╲ (_│ │ │_) │ │_│ │_│ │ │ ╲ ╲   00110000 |
  | │_│  ╲_╲__,_│ .__╱ ╲__│╲___╱│_│  ╲_╲  01010010 |
  |             │ │ A fast and simple C++ 00010100 |
  |             │_│ binary serialization library   |
  +------------------------------------------------+
*/

#pragma once
#include "Defines.hpp"
#include "RptrMemory.hpp"
#include "TypeTraits/TypeTraits.hpp"
#include "TypeTraits/Vector.hpp"
#include "TypeTraits/Funcs.hpp"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>


namespace Rapt0R
{
    class Writer
    {
    public:
        explicit Writer(size_t initialCapacity = 4096) noexcept
            : mStart(nullptr), mCur(nullptr), mEnd(nullptr), mCapacityEnd(nullptr), mStatus(ErrorCode::SUCCESS), mOwnsMemory(true)
        {
            if(initialCapacity > 0)
                Reserve(initialCapacity);
        }

        constexpr explicit Writer(std::span<Byte> externalBuffer) noexcept
            : mStart(externalBuffer.data()), mCur(externalBuffer.data()), mEnd(externalBuffer.data() + externalBuffer.size()),
              mCapacityEnd(externalBuffer.data() + externalBuffer.size()), mStatus(ErrorCode::SUCCESS), mOwnsMemory(false) {}

        constexpr Writer(void* data, size_t sizeBytes) noexcept
            : mStart(static_cast<Byte*>(data)), mCur(static_cast<Byte*>(data)), mEnd(static_cast<Byte*>(data) + sizeBytes),
              mCapacityEnd(static_cast<Byte*>(data) + sizeBytes), mStatus(ErrorCode::SUCCESS), mOwnsMemory(false) {}

        ~Writer() noexcept
        {
            if(mOwnsMemory && mStart)
                Rapt0R::Free(mStart);
        }

        Writer(const Writer&) = delete;
        Writer& operator=(const Writer&) = delete;

        constexpr Writer(Writer&& other) noexcept
            : mStart(other.mStart), mCur(other.mCur), mEnd(other.mEnd), mCapacityEnd(other.mCapacityEnd), mStatus(other.mStatus), mOwnsMemory(other.mOwnsMemory)
        {
            other.mStart = nullptr;
            other.mCur = nullptr;
            other.mEnd = nullptr;
            other.mCapacityEnd = nullptr;
            other.mOwnsMemory = false;
        }

        constexpr Writer& operator=(Writer&& other) noexcept
        {
            if(this != &other)
            {
                if(mOwnsMemory && mStart)
                    std::free(mStart);

                mStart = other.mStart;
                mCur = other.mCur;
                mEnd = other.mEnd;
                mCapacityEnd = other.mCapacityEnd;
                mStatus = other.mStatus;
                mOwnsMemory = other.mOwnsMemory;

                other.mStart = nullptr;
                other.mCur = nullptr;
                other.mEnd = nullptr;
                other.mCapacityEnd = nullptr;
                other.mOwnsMemory = false;
            }
            return *this;
        }

        R0R_ND R0R_INLINE constexpr const Byte* Data() const noexcept                  { return mStart; }
        R0R_ND R0R_INLINE constexpr Byte* Data() noexcept                              { return mStart; }
        R0R_ND R0R_INLINE constexpr size_t Size() const noexcept                       { return static_cast<size_t>(mCur - mStart); }
        R0R_ND R0R_INLINE constexpr size_t Capacity() const noexcept                   { return static_cast<size_t>(mCapacityEnd - mStart); }
        R0R_ND R0R_INLINE constexpr size_t RemainingCapacity() const noexcept          { return static_cast<size_t>(mEnd - mCur); }
        R0R_ND R0R_INLINE constexpr bool HasError() const noexcept                     { return mStatus != ErrorCode::SUCCESS; }
        R0R_ND R0R_INLINE constexpr ErrorCode Status() const noexcept                  { return mStatus; }
        R0R_ND R0R_INLINE constexpr std::span<const Byte> WrittenSpan() const noexcept { return { mStart, Size() }; }
        R0R_ND R0R_INLINE constexpr explicit operator bool() const noexcept            { return !HasError(); }

        // Records the first error only, so custom serializers can report failures through the same sticky status.
        // Also closes the write window (mEnd = mCur): every later write then fails its capacity check and lands in the
        // cold Grow() path, which is why the hot paths do not need a separate HasError() branch.
        R0R_INLINE constexpr void SetError(ErrorCode code) noexcept
        {
            if(mStatus == ErrorCode::SUCCESS)
                mStatus = code;
            mEnd = mCur;
        }

        // Fails with SIZE_OVERFLOW if a length or element count cannot be encoded in the uint32_t wire format
        R0R_ND R0R_INLINE bool ValidateCount(size_t count) noexcept
        {
            if(count > MAX_ELEMENT_COUNT) [[unlikely]]
            {
                SetError(ErrorCode::SIZE_OVERFLOW);
                return false;
            }
            return true;
        }

        R0R_INLINE void Clear() noexcept
        {
            mCur = mStart;
            mEnd = mCapacityEnd;
            mStatus = ErrorCode::SUCCESS;
        }

        bool Reserve(size_t newCapacity) noexcept
        {
            if(newCapacity <= Capacity())
                return true;

            if(!mOwnsMemory) [[unlikely]]
            {
                SetError(ErrorCode::BUFFER_OVERFLOW);
                return false;
            }

            const size_t currentOffset = Size();
            Byte* newBlock = static_cast<Byte*>(Rapt0R::Realloc(mStart, newCapacity));
            if(!newBlock) [[unlikely]]
            {
                SetError(ErrorCode::BUFFER_OVERFLOW);
                return false;
            }

            mStart = newBlock;
            mCur = mStart + currentOffset;
            mCapacityEnd = mStart + newCapacity;
            mEnd = HasError() ? mCur : mCapacityEnd; // Growing never reopens a writer that already failed
            return true;
        }

        R0R_ND R0R_INLINE bool EnsureCapacity(size_t extraBytes) noexcept
        {
            if(RemainingCapacity() < extraBytes) [[unlikely]]
                return Grow(extraBytes);

            return true;
        }

        // Fast power-of-two cursor alignment (Zero-fills padding bytes for deterministic output)
        // Alignment is relative to the start of the stream, which is what the Reader reproduces
        R0R_INLINE bool Align(size_t alignment) noexcept
        {
            if(alignment == 0 || (alignment & (alignment - 1)) != 0) [[unlikely]]
            {
                SetError(ErrorCode::FAILED);
                return false;
            }

            const size_t offset = Size();
            const size_t alignedOffset = (offset + (alignment - 1)) & ~(alignment - 1);
            const size_t padding = alignedOffset - offset;

            if(padding == 0)
                return true;

            if(!EnsureCapacity(padding)) [[unlikely]]
                return false;

            std::memset(mCur, 0, padding);
            mCur += padding;
            return true;
        }

        // Zero-overhead unchecked write (Call ONLY after EnsureCapacity!)
        template <typename T> requires IsPODBlittable_V<T>
        R0R_INLINE void WriteUnchecked(const T& val) noexcept
        {
            Rapt0R::Memcopy<sizeof(T)>(mCur, &val);
            mCur += sizeof(T);
        }

        R0R_INLINE bool WriteRawBytes(const void* source, size_t size) noexcept
        {
            if(!EnsureCapacity(size)) [[unlikely]]
                return false;

            if(size > 0)
            {
                Rapt0R::Memcopy(mCur, source, size);
                mCur += size;
            }
            return true;
        }

        R0R_INLINE bool WriteString(std::string_view str) noexcept
        {
            if(!ValidateCount(str.size())) [[unlikely]]
                return false;

            const uint32_t len = static_cast<uint32_t>(str.size());
            const size_t totalBytes = sizeof(uint32_t) + len;

            // Single capacity check for both the length prefix and string payload!
            if(!EnsureCapacity(totalBytes)) [[unlikely]]
                return false;

            Rapt0R::Memcopy<sizeof(uint32_t)>(mCur, &len);
            mCur += sizeof(uint32_t);

            Rapt0R::MemcopyVariable(mCur, str.data(), len);
            mCur += len;
            return true;
        }

        template <typename T> requires IsPODBlittable_V<T>
        R0R_INLINE bool WriteSpan(std::span<const T> span, bool alignToType = false) noexcept
        {
            if(!ValidateCount(span.size())) [[unlikely]]
                return false;

            const uint32_t count = static_cast<uint32_t>(span.size());
            if(!WriteRawBytes(&count, sizeof(uint32_t))) [[unlikely]]
                return false;

            // The Reader also skips alignment for empty spans, both sides must agree on padding
            if(count == 0)
                return true;

            if(alignToType && !Align(alignof(T))) [[unlikely]]
                return false;

            const size_t totalBytes = static_cast<size_t>(count) * sizeof(T);
            return WriteRawBytes(span.data(), totalBytes);
        }

        template <typename T, typename Alloc>
        bool WriteVector(const std::vector<T, Alloc>& inVec) noexcept
        {
            if(!ValidateCount(inVec.size())) [[unlikely]]
                return false;

            const uint32_t count = static_cast<uint32_t>(inVec.size());

            if constexpr(std::is_same_v<T, bool>)
            {
                // std::vector<bool> is bit-packed and has no data(), so write one byte per element
                if(!EnsureCapacity(sizeof(uint32_t) + count)) [[unlikely]]
                    return false;

                WriteUnchecked(count);
                for(const bool item : inVec)
                    WriteUnchecked(static_cast<uint8_t>(item));
                return true;
            }
            else if constexpr(IsRawCopyable_V<T>)
            {
                const size_t payloadBytes = static_cast<size_t>(count) * sizeof(T);
                const size_t totalBytes = sizeof(uint32_t) + payloadBytes;

                // Single bounds check for count + entire contiguous vector array
                if(!EnsureCapacity(totalBytes)) [[unlikely]]
                    return false;

                Rapt0R::Memcopy<sizeof(uint32_t)>(mCur, &count);
                mCur += sizeof(uint32_t);

                if(payloadBytes > 0)
                {
                    Rapt0R::Memcopy(mCur, inVec.data(), payloadBytes);
                    mCur += payloadBytes;
                }
                return true;
            }
            else if constexpr(WriterFlatKind_V<T> != FlatKind::NONE)
            {
                // e.g. std::vector<std::string>: one capacity check for the whole vector while it is small
                return WriteFlatContainer(inVec);
            }
            else
            {
                if(!WriteRawBytes(&count, sizeof(uint32_t))) [[unlikely]]
                    return false;

                for(const auto& item : inVec)
                {
                    WriteValue(item);
                    if(HasError()) [[unlikely]]
                        return false;
                }
                return true;
            }
        }

        // Fused multi-value write, the fastest way to serialize a struct:  w.Write(id, name, position, assets, ...)
        // Produces exactly the same bytes as separate w.Write(a), w.Write(b), w.Write(c) calls, but:
        //  - each run of consecutive "flat" values (raw PODs, strings, raw vectors, raw optionals) shares ONE capacity check
        //  - a run is copied through a local cursor the compiler keeps in a register. Through the member cursor, every
        //    memcpy into Byte (an aliasing type) would force a reload/store of mCur/mEnd, serializing the writes.
        // Non-flat values (maps, nested structs, ...) split the runs at compile time and go through WriteValue().
        template <typename... Ts> requires (sizeof...(Ts) > 0)
        R0R_INLINE bool Write(const Ts&... values) noexcept
        {
            return WriteRunsFrom<0>(values...);
        }

        // Count + every element of a small container of flat elements (or flat key/value pairs, for maps) under a single
        // capacity check. Sizes the container in a first pass, so large containers fall back to one check per element
        // rather than walking their nodes twice.
        template <typename Container>
        bool WriteFlatContainer(const Container& container) noexcept
        {
            constexpr size_t TWO_PASS_LIMIT = SMALL_CONTAINER_LIMIT;

            if(!ValidateCount(container.size())) [[unlikely]]
                return false;

            const uint32_t count = static_cast<uint32_t>(container.size());
            if(count > TWO_PASS_LIMIT)
            {
                if(!WritePODs(count)) [[unlikely]]
                    return false;
                for(const auto& element : container)
                {
                    if(!WriteElement(element)) [[unlikely]]
                        return false;
                }
                return true;
            }

            size_t totalBytes = sizeof(uint32_t);
            for(const auto& element : container)
                totalBytes += ElementFlatSize(element);

            if(!EnsureCapacity(totalBytes)) [[unlikely]]
                return false;

            Byte* cur = mCur;
            WriteFlat(cur, count);
            for(const auto& element : container)
                WriteElementFlat(cur, element);
            mCur = cur;
            return true;
        }

        // Variadic POD Fast-Path: 1 capacity check for N variables!
        template <typename... Ts> requires (sizeof...(Ts) > 0) && (IsPODBlittable_V<std::remove_cvref_t<Ts>> && ...)
        R0R_INLINE bool WritePODs(const Ts&... args) noexcept
        {
            constexpr size_t totalBytes = (sizeof(Ts) + ... + 0);

            if(!EnsureCapacity(totalBytes)) [[unlikely]]
                return false;

            (WriteUnchecked(args), ...);
            return true;
        }
    private:
        // Serializes a single value. Write() routes every non-flat value through here, callers check HasError().
        template <typename T>
        R0R_INLINE void WriteValue(const T& source) noexcept
        {
            using RawType = std::remove_cvref_t<T>;

            // User-provided serializers come first, so they are never bypassed by the raw blit path.
            // Only these (potentially heavy) paths check the error flag, every other path fails its capacity check
            // once the writer has an error.
            if constexpr(HasMemberSerialize<RawType, Writer>)
            {
                if(!HasError()) [[likely]]
                    source.RAPTOR_SERIALIZE_FUNC(*this);
            }
            else if constexpr(HasFreeSerialize<RawType, Writer>)
            {
                if(!HasError()) [[likely]]
                    RAPTOR_SERIALIZE_FUNC(*this, source);
            }
            else if constexpr(std::is_same_v<RawType, std::string_view> || IsStdString_V<RawType>)
                WriteString(std::string_view(source));
            else if constexpr(IsCharArray_V<RawType>)
            {
                // Stop at the first null terminator inside the array (string literals end with one)
                constexpr size_t capacity = std::extent_v<RawType>;
                const char* terminator = std::char_traits<char>::find(source, capacity, '\0');
                WriteString(std::string_view(source, terminator ? static_cast<size_t>(terminator - source) : capacity));
            }
            else if constexpr(IsCharPointer_V<RawType>)
                WriteString(source ? std::string_view(source) : std::string_view());
            else if constexpr(IsStandardVector_V<RawType>)
                WriteVector(source);
            else if constexpr(IsRawCopyable_V<RawType>)
                WriteRawBytes(&source, sizeof(RawType));
            else
                static_assert(sizeof(T) == 0, "Type does not support serialization via Rapt0R::Writer (a type with a Rapt0RDeserialize also needs a Rapt0RSerialize)");
        }

        // Wire size of a flat value. Never 0: on a size overflow ValidateCount() closes the write window and this still
        // returns the prefix size, so the run's capacity check fails without a separate HasError() branch.
        template <typename T>
        R0R_INLINE size_t FlatSize(const T& value) noexcept
        {
            constexpr FlatKind kind = WriterFlatKind_V<T>;
            static_assert(kind != FlatKind::NONE);
            if constexpr(kind == FlatKind::RAW || kind == FlatKind::BOOL)
                return sizeof(T);
            else if constexpr(kind == FlatKind::STRING || kind == FlatKind::RAW_VECTOR)
                return sizeof(uint32_t) + (ValidateCount(value.size()) ? value.size() * sizeof(*value.data()) : 0);
            else if constexpr(kind == FlatKind::RAW_OPTIONAL)
                return sizeof(uint8_t) + (value.has_value() ? sizeof(typename T::value_type) : 0);
            else
            {
                size_t bytes = sizeof(uint32_t);
                if(ValidateCount(value.size())) [[likely]]
                {
                    for(const auto& element : value)
                        bytes += ElementFlatSize(element);
                }
                return bytes;
            }
        }

        // Containers are sized by walking them, which only pays off while they are small
        template <typename T>
        R0R_INLINE static bool IsCheapToSize(const T& value) noexcept
        {
            if constexpr(WriterFlatKind_V<T> == FlatKind::FLAT_CONTAINER)
                return value.size() <= SMALL_CONTAINER_LIMIT;
            else
                return true;
        }

        // Copies one flat value at `cur`, the caller has reserved FlatSize(value) bytes
        template <typename T>
        R0R_INLINE static void WriteFlat(Byte*& cur, const T& value) noexcept
        {
            constexpr FlatKind kind = WriterFlatKind_V<T>;
            if constexpr(kind == FlatKind::RAW || kind == FlatKind::BOOL)
            {
                Rapt0R::Memcopy<sizeof(T)>(cur, &value);
                cur += sizeof(T);
            }
            else if constexpr(kind == FlatKind::STRING || kind == FlatKind::RAW_VECTOR)
            {
                const uint32_t count = static_cast<uint32_t>(value.size());
                Rapt0R::Memcopy<sizeof(uint32_t)>(cur, &count);
                cur += sizeof(uint32_t);

                const size_t payloadBytes = value.size() * sizeof(*value.data());
                Rapt0R::MemcopyVariable(cur, value.data(), payloadBytes);
                cur += payloadBytes;
            }
            else if constexpr(kind == FlatKind::RAW_OPTIONAL)
            {
                *cur++ = value.has_value() ? 1 : 0;
                if(value.has_value())
                {
                    Rapt0R::Memcopy<sizeof(typename T::value_type)>(cur, &*value);
                    cur += sizeof(typename T::value_type);
                }
            }
            else
            {
                const uint32_t count = static_cast<uint32_t>(value.size());
                Rapt0R::Memcopy<sizeof(uint32_t)>(cur, &count);
                cur += sizeof(uint32_t);
                for(const auto& element : value)
                    WriteElementFlat(cur, element);
            }
        }

        // Container elements: maps iterate std::pair<const K, V>, written as key then value
        template <typename E>
        R0R_INLINE size_t ElementFlatSize(const E& element) noexcept
        {
            if constexpr(IsStdPair_V<E>)
                return FlatSize(element.first) + FlatSize(element.second);
            else
                return FlatSize(element);
        }

        template <typename E>
        R0R_INLINE static void WriteElementFlat(Byte*& cur, const E& element) noexcept
        {
            if constexpr(IsStdPair_V<E>)
            {
                WriteFlat(cur, element.first);
                WriteFlat(cur, element.second);
            }
            else
                WriteFlat(cur, element);
        }

        template <typename E>
        R0R_INLINE bool WriteElement(const E& element) noexcept
        {
            if constexpr(IsStdPair_V<E>)
                return Write(element.first, element.second);
            else
                return Write(element);
        }

        // Index of the first non-flat value at or after BEGIN (COUNT if the rest is flat)
        template <size_t BEGIN, typename... Ts>
        static consteval size_t FirstNonFlat()
        {
            constexpr FlatKind kinds[] = { WriterFlatKind_V<Ts>... };
            for(size_t i = BEGIN; i < sizeof...(Ts); ++i)
            {
                if(kinds[i] == FlatKind::NONE)
                    return i;
            }
            return sizeof...(Ts);
        }

        // One run of flat values: a single capacity check (which also catches a closed window), then straight-line copies.
        // A run holding a large container is written value by value instead (same bytes), to avoid walking it twice.
        template <size_t OFFSET, size_t... Is, typename... Ts>
        R0R_INLINE bool WriteFlatRun(std::index_sequence<Is...>, const Ts&... values) noexcept
        {
            constexpr bool HAS_CONTAINER = ((WriterFlatKind_V<NthType<OFFSET + Is, Ts...>> == FlatKind::FLAT_CONTAINER) || ...);
            if constexpr(HAS_CONTAINER)
            {
                if(!(IsCheapToSize(NthArg<OFFSET + Is>(values...)) && ...)) [[unlikely]]
                {
                    (WriteValue(NthArg<OFFSET + Is>(values...)), ...);
                    return !HasError();
                }
            }

            const size_t totalBytes = (FlatSize(NthArg<OFFSET + Is>(values...)) + ...);
            if(!EnsureCapacity(totalBytes)) [[unlikely]]
                return false;

            Byte* cur = mCur;
            (WriteFlat(cur, NthArg<OFFSET + Is>(values...)), ...);
            mCur = cur;
            return true;
        }

        template <size_t BEGIN, typename... Ts>
        R0R_INLINE bool WriteRunsFrom(const Ts&... values) noexcept
        {
            constexpr size_t COUNT = sizeof...(Ts);
            if constexpr(BEGIN == COUNT)
                return true;
            else
            {
                constexpr size_t RUN_END = FirstNonFlat<BEGIN, Ts...>();
                if constexpr(RUN_END > BEGIN)
                {
                    if(!WriteFlatRun<BEGIN>(std::make_index_sequence<RUN_END - BEGIN> {}, values...)) [[unlikely]]
                        return false;
                }

                if constexpr(RUN_END < COUNT)
                {
                    WriteValue(NthArg<RUN_END>(values...));
                    if(HasError()) [[unlikely]]
                        return false;
                    return WriteRunsFrom<RUN_END + 1>(values...);
                }
                else
                    return true;
            }
        }

        R0R_NOINLINE bool Grow(size_t extraBytes) noexcept
        {
            // A failed writer has a closed window, so every write after an error ends up here and fails
            if(HasError()) [[unlikely]]
                return false;

            if(!mOwnsMemory) [[unlikely]]
            {
                SetError(ErrorCode::BUFFER_OVERFLOW);
                return false;
            }

            const size_t currentSize = Size();
            if(extraBytes > SIZE_MAX - currentSize) [[unlikely]]
            {
                SetError(ErrorCode::BUFFER_OVERFLOW);
                return false;
            }

            const size_t currentCap = Capacity();
            const size_t requiredCap = currentSize + extraBytes;

            // Geometric 2x growth to amortize allocations (Saturates instead of overflowing)
            size_t newCap = currentCap > 0 ? (currentCap <= SIZE_MAX / 2 ? currentCap * 2 : SIZE_MAX) : 4096;
            if(newCap < requiredCap)
                newCap = requiredCap;

            return Reserve(newCap);
        }

    private:
        Byte* mStart;
        Byte* mCur;
        Byte* mEnd;         // Write window end, equals mCapacityEnd until an error closes the window
        Byte* mCapacityEnd; // True end of the buffer
        ErrorCode mStatus;
        bool mOwnsMemory;
    };
}
