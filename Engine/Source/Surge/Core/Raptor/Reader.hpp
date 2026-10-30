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

#include <array>
#include <cstdint>
#include <cstring>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace Rapt0R
{
    class Reader
    {
    public:
        constexpr explicit Reader(std::span<const Byte> buffer) noexcept
            : mStart(buffer.data()), mCur(buffer.data()), mEnd(buffer.data() + buffer.size()), mBufferEnd(buffer.data() + buffer.size()), mStatus(ErrorCode::SUCCESS) {}

        constexpr Reader(const void* data, size_t sizeBytes) noexcept
            : mStart(static_cast<const Byte*>(data)), mCur(static_cast<const Byte*>(data)), mEnd(static_cast<const Byte*>(data) + sizeBytes),
              mBufferEnd(static_cast<const Byte*>(data) + sizeBytes), mStatus(ErrorCode::SUCCESS) {}

        R0R_ND R0R_INLINE constexpr size_t Cursor() const noexcept                    { return static_cast<size_t>(mCur - mStart); }
        R0R_ND R0R_INLINE constexpr size_t RemainingBytes() const noexcept            { return static_cast<size_t>(mEnd - mCur); }
        R0R_ND R0R_INLINE constexpr bool HasError() const noexcept                    { return mStatus != ErrorCode::SUCCESS; }
        R0R_ND R0R_INLINE constexpr ErrorCode Status() const noexcept                 { return mStatus; }
        R0R_ND R0R_INLINE constexpr std::span<const Byte> UnreadSpan() const noexcept { return { mCur, RemainingBytes() }; }
        R0R_ND R0R_INLINE constexpr explicit operator bool() const noexcept           { return !HasError(); }
        R0R_ND R0R_INLINE bool EnsureRemaining(size_t bytes) noexcept                 { return !IsOverflowing(bytes); }

        // Records the first error only, so custom deserializers can report failures through the same sticky status.
        // Also closes the read window (mEnd = mCur): every later read then fails its bounds check, which is why the
        // hot paths do not need a separate HasError() branch.
        R0R_INLINE constexpr void SetError(ErrorCode code) noexcept
        {
            if(mStatus == ErrorCode::SUCCESS)
                mStatus = code;
            mEnd = mCur;
        }

        // Rejects element counts the remaining input cannot possibly hold, BEFORE anything is allocated for them.
        // Division instead of multiplication, so a hostile count cannot overflow size_t on 32-bit targets.
        R0R_ND R0R_INLINE bool EnsureElements(size_t count, size_t minBytesPerElement = 1) noexcept
        {
            if(count > RemainingBytes() / minBytesPerElement) [[unlikely]]
            {
                SetError(ErrorCode::BUFFER_OVERFLOW);
                return false;
            }
            return true;
        }

        // Fast power-of-two cursor alignment (Crucial for zero-copy ARM64 / SIMD spans)
        // Alignment is relative to the start of the stream, matching the Writer
        R0R_INLINE bool Align(size_t alignment) noexcept
        {
            if(alignment == 0 || (alignment & (alignment - 1)) != 0) [[unlikely]]
            {
                SetError(ErrorCode::FAILED);
                return false;
            }

            const size_t offset = Cursor();
            const size_t alignedOffset = (offset + (alignment - 1)) & ~(alignment - 1);
            const size_t padding = alignedOffset - offset;

            if(IsOverflowing(padding)) [[unlikely]]
                return false;

            mCur += padding;
            return true;
        }

        R0R_INLINE bool Skip(size_t byteCount) noexcept
        {
            if(IsOverflowing(byteCount)) [[unlikely]]
                return false;

            mCur += byteCount;
            return true;
        }

        R0R_INLINE bool Seek(size_t absolutePosition) noexcept
        {
            // Seeking must not reopen a reader whose window was closed by an error
            if(HasError()) [[unlikely]]
                return false;

            const size_t totalSize = static_cast<size_t>(mBufferEnd - mStart);
            if(absolutePosition > totalSize) [[unlikely]]
            {
                SetError(ErrorCode::BUFFER_OVERFLOW);
                return false;
            }
            mCur = mStart + absolutePosition;
            return true;
        }

        // Zero-overhead unchecked read (Call ONLY after EnsureRemaining!)
        template <typename T> requires IsPODBlittable_V<T>
        R0R_ND R0R_INLINE T ReadUnchecked() noexcept
        {
            T val;
            Rapt0R::Memcopy<sizeof(T)>(&val, mCur);
            mCur += sizeof(T);
            return val;
        }

        R0R_INLINE bool ReadRawBytes(void* destination, size_t size) noexcept
        {
            if(IsOverflowing(size)) [[unlikely]]
                return false;

            if(size > 0)
            {
                Rapt0R::Memcopy(destination, mCur, size);
                mCur += size;
            }
            return true;
        }

        R0R_INLINE bool ReadStringView(std::string_view& outView) noexcept
        {
            uint32_t len = 0;
            if(!ReadRawBytes(&len, sizeof(uint32_t))) [[unlikely]]
                return false;

            if(IsOverflowing(len)) [[unlikely]]
                return false;

            outView = std::string_view(reinterpret_cast<const char*>(mCur), len);
            mCur += len;
            return true;
        }

        template <typename Alloc>
        bool ReadString(std::basic_string<char, std::char_traits<char>, Alloc>& target)
        {
            uint32_t len = 0;
            if(!ReadRawBytes(&len, sizeof(uint32_t))) [[unlikely]]
                return false;

            if(IsOverflowing(len)) [[unlikely]]
                return false;

            AssignString(target, mCur, len);
            mCur += len;
            return true;
        }

        // Reads a length-prefixed string into a fixed char buffer, always null-terminated
        template <size_t N>
        bool ReadCharArray(char (&target)[N]) noexcept
        {
            std::string_view view;
            if(!ReadStringView(view)) [[unlikely]]
                return false;

            if(view.size() >= N) [[unlikely]]
            {
                SetError(ErrorCode::BUFFER_OVERFLOW);
                return false;
            }

            if(!view.empty())
                Rapt0R::Memcopy(target, view.data(), view.size());
            target[view.size()] = '\0';
            return true;
        }

        // Zero-copy span extraction with optional hardware alignment.
        // Fails with MISALIGNED_DATA instead of handing out a misaligned T* (UB, faults on some ARM/SIMD loads).
        // If the source buffer itself is not aligned, use ReadVector to copy instead.
        template <typename T>
        requires IsPODBlittable_V<T>
        R0R_INLINE bool ReadSpan(std::span<const T>& outSpan, bool alignToType = false) noexcept
        {
            uint32_t count = 0;
            if(!ReadRawBytes(&count, sizeof(uint32_t))) [[unlikely]]
                return false;

            // The Writer skips alignment for empty spans, both sides must agree on padding
            if(count == 0)
            {
                outSpan = {};
                return true;
            }

            if(alignToType && !Align(alignof(T))) [[unlikely]]
                return false;

            if(!EnsureElements(count, sizeof(T))) [[unlikely]]
                return false;

            if constexpr(alignof(T) > 1)
            {
                if(reinterpret_cast<uintptr_t>(mCur) % alignof(T) != 0) [[unlikely]]
                {
                    SetError(ErrorCode::MISALIGNED_DATA);
                    return false;
                }
            }

            outSpan = std::span<const T>(reinterpret_cast<const T*>(mCur), count);
            mCur += static_cast<size_t>(count) * sizeof(T);
            return true;
        }

        template <typename T, typename Alloc>
        bool ReadVector(std::vector<T, Alloc>& outVec)
        {
            uint32_t count = 0;
            if(!ReadRawBytes(&count, sizeof(uint32_t))) [[unlikely]]
                return false;

            if(count == 0)
            {
                outVec.clear();
                return true;
            }

            if constexpr(std::is_same_v<T, bool>)
            {
                // std::vector<bool> is written as one validated byte per element
                if(!EnsureElements(count, sizeof(uint8_t))) [[unlikely]]
                    return false;

                outVec.resize(count);
                for(uint32_t i = 0; i < count; ++i)
                {
                    const uint8_t value = ReadUnchecked<uint8_t>();
                    if(value > 1) [[unlikely]]
                    {
                        SetError(ErrorCode::CORRUPT_DATA);
                        return false;
                    }
                    outVec[i] = value != 0;
                }
                return true;
            }
            else if constexpr(IsRawCopyable_V<T>)
            {
                if(!EnsureElements(count, sizeof(T))) [[unlikely]]
                    return false;

                // resize() zero-fills new elements before the memcpy overwrites them,
                // use Rapt0R::DefaultInitAllocator to skip that for freshly allocated vectors
                const size_t totalBytes = static_cast<size_t>(count) * sizeof(T);
                outVec.resize(count);
                Rapt0R::Memcopy(outVec.data(), mCur, totalBytes);
                mCur += totalBytes;
                return true;
            }
            else
            {
                // Every element occupies at least one byte, so this bounds the allocation by the input size
                if(!EnsureElements(count)) [[unlikely]]
                    return false;

                outVec.resize(count); // Reuse existing element capacity
                for(uint32_t i = 0; i < count; ++i)
                {
                    ReadValue(outVec[i]);
                    if(HasError()) [[unlikely]]
                        return false;
                }
                return true;
            }
        }

        // Fused multi-value read, the counterpart of Writer::Write:  r.Read(id, name, position, assets, ...)
        // Consumes exactly the bytes of separate r.Read(a), r.Read(b), r.Read(c) calls. Each run of flat values gets one
        // up-front bounds check covering the minimum size of the whole run (computed at compile time) plus one check per
        // variable-length payload, all through a local cursor the compiler keeps in a register. Non-flat values split the runs and use ReadValue().
        template <typename... Ts> requires (sizeof...(Ts) > 0)
        R0R_INLINE bool Read(Ts&... values)
        {
            return ReadRunsFrom<0>(values...);
        }

        template <typename T> requires IsPODBlittable_V<T>
        R0R_INLINE void ReadUncheckedInto(T& target) noexcept
        {
            Rapt0R::Memcopy<sizeof(T)>(&target, mCur);
            mCur += sizeof(T);
        }

        template <typename... Ts> requires (sizeof...(Ts) > 0) && (IsPODBlittable_V<std::remove_cvref_t<Ts>> && ...)
        R0R_INLINE bool ReadPODs(Ts&... args) noexcept
        {
            constexpr size_t totalBytes = (sizeof(Ts) + ... + 0);

            if(IsOverflowing(totalBytes)) [[unlikely]]
                return false;

            (ReadUncheckedInto(args), ...);
            return true;
        }
    private:
        // Deserializes a single value. Read() routes every non-flat value through here, callers check HasError().
        template <typename T>
        R0R_INLINE void ReadValue(T& target)
        {
            using RawType = std::remove_cvref_t<T>;

            // User-provided deserializers come first, so they are never bypassed by the raw blit path.
            // Only these (potentially heavy) paths check the error flag, every other path fails its bounds check
            // once the reader has an error.
            if constexpr(HasMemberDeserialize<RawType, Reader>)
            {
                if(!HasError()) [[likely]]
                    target.RAPTOR_DESERIALIZE_FUNC(*this);
            }
            else if constexpr(HasFreeDeserialize<RawType, Reader>)
            {
                if(!HasError()) [[likely]]
                    RAPTOR_DESERIALIZE_FUNC(*this, target);
            }
            else if constexpr(std::is_same_v<RawType, std::string_view>)
                ReadStringView(target);
            else if constexpr(IsStdString_V<RawType>)
                ReadString(target);
            else if constexpr(IsCharArray_V<RawType>)
                ReadCharArray(target);
            else if constexpr(IsStandardVector_V<RawType>)
                ReadVector(target);
            else if constexpr(std::is_same_v<RawType, bool>)
            {
                // Any byte other than 0/1 would be an invalid bool object (UB)
                uint8_t value = 0;
                if(ReadRawBytes(&value, sizeof(uint8_t)))
                {
                    if(value > 1) [[unlikely]]
                        SetError(ErrorCode::CORRUPT_DATA);
                    else
                        target = value != 0;
                }
            }
            else if constexpr(IsRawCopyable_V<RawType>)
                ReadRawBytes(&target, sizeof(RawType));
            else
                static_assert(sizeof(T) == 0, "Type does not support deserialization via Rapt0R::Reader (a type with a Rapt0RSerialize also needs a Rapt0RDeserialize)");
        }

        template <typename Alloc>
        R0R_INLINE static void AssignString(std::basic_string<char, std::char_traits<char>, Alloc>& target, const Byte* source, size_t len)
        {
#if __cpp_lib_string_resize_and_overwrite >= 202110L
            // C++23: Avoids double zero-initialization before memcpy
            target.resize_and_overwrite(len, [source](char* buf, size_t n) noexcept { Rapt0R::MemcopyVariable(buf, source, n); return n; });
#else
            target.assign(reinterpret_cast<const char*>(source), len);
#endif
        }

        // Smallest number of bytes a flat value can occupy on the wire. Never 0, so a run's up-front check also fails
        // on a reader whose window was closed by an earlier error.
        template <typename T>
        static consteval size_t FlatMinSize()
        {
            constexpr FlatKind kind = FlatKind_V<T>;
            static_assert(kind != FlatKind::NONE);
            if constexpr(kind == FlatKind::RAW)
                return sizeof(T);
            else if constexpr(kind == FlatKind::BOOL || kind == FlatKind::RAW_OPTIONAL)
                return sizeof(uint8_t);
            else
                return sizeof(uint32_t);
        }

        // Reads one flat value at `cur`. The caller guarantees at least FlatMinSize<T>() + SUFFIX_MIN bytes remain, where
        // SUFFIX_MIN is the minimum size of the rest of the run. Variable-length payloads are checked against the bytes
        // left over after reserving SUFFIX_MIN, so later fixed-size values never need their own check.
        template <size_t SUFFIX_MIN, typename T>
        R0R_INLINE bool ReadFlat(const Byte*& cur, const Byte* end, T& value)
        {
            constexpr FlatKind kind = FlatKind_V<T>;
            if constexpr(kind == FlatKind::RAW)
            {
                Rapt0R::Memcopy<sizeof(T)>(&value, cur);
                cur += sizeof(T);
            }
            else if constexpr(kind == FlatKind::BOOL)
            {
                const uint8_t byte = *cur;
                if(byte > 1) [[unlikely]]
                {
                    SetError(ErrorCode::CORRUPT_DATA);
                    return false;
                }
                value = byte != 0;
                ++cur;
            }
            else if constexpr(kind == FlatKind::STRING || kind == FlatKind::RAW_VECTOR)
            {
                uint32_t count = 0;
                Rapt0R::Memcopy<sizeof(uint32_t)>(&count, cur);
                cur += sizeof(uint32_t);

                constexpr size_t ELEMENT_SIZE = sizeof(*value.data());
                const size_t available = static_cast<size_t>(end - cur) - SUFFIX_MIN;
                if(count > available / ELEMENT_SIZE) [[unlikely]]
                {
                    SetError(ErrorCode::BUFFER_OVERFLOW);
                    return false;
                }

                const size_t payloadBytes = static_cast<size_t>(count) * ELEMENT_SIZE;
                if constexpr(std::is_same_v<T, std::string_view>)
                    value = std::string_view(reinterpret_cast<const char*>(cur), count);
                else if constexpr(kind == FlatKind::STRING)
                    AssignString(value, cur, count);
                else
                {
                    value.resize(count);
                    Rapt0R::MemcopyVariable(value.data(), cur, payloadBytes);
                }
                cur += payloadBytes;
            }
            else if constexpr(kind == FlatKind::RAW_OPTIONAL)
            {
                using E = typename T::value_type;
                const uint8_t flag = *cur++;
                if(flag > 1) [[unlikely]]
                {
                    SetError(ErrorCode::CORRUPT_DATA);
                    return false;
                }

                if(flag)
                {
                    if(sizeof(E) > static_cast<size_t>(end - cur) - SUFFIX_MIN) [[unlikely]]
                    {
                        SetError(ErrorCode::BUFFER_OVERFLOW);
                        return false;
                    }
                    if(!value.has_value())
                        value.emplace();
                    Rapt0R::Memcopy<sizeof(E)>(&*value, cur);
                    cur += sizeof(E);
                }
                else
                {
                    value.reset();
                }
            }
            return true;
        }

        // Index of the first non-flat value at or after BEGIN (COUNT if the rest is flat)
        template <size_t BEGIN, typename... Ts>
        static consteval size_t FirstNonFlat()
        {
            constexpr FlatKind kinds[] = { FlatKind_V<Ts>... };
            for(size_t i = BEGIN; i < sizeof...(Ts); ++i)
            {
                if(kinds[i] == FlatKind::NONE)
                    return i;
            }
            return sizeof...(Ts);
        }

        template <size_t OFFSET, size_t... Is, typename... Ts>
        R0R_INLINE bool ReadFlatRun(std::index_sequence<Is...>, Ts&... values)
        {
            constexpr size_t COUNT = sizeof...(Is);
            static constexpr std::array<size_t, COUNT + 1> SUFFIX_MIN = [] {
                constexpr size_t mins[COUNT] = { FlatMinSize<NthType<OFFSET + Is, Ts...>>()... };
                std::array<size_t, COUNT + 1> suffix {};
                for(size_t i = COUNT; i-- > 0;)
                    suffix[i] = suffix[i + 1] + mins[i];
                return suffix;
            }();

            if(IsOverflowing(SUFFIX_MIN[0])) [[unlikely]]
                return false;

            const Byte* cur = mCur;
            const Byte* const end = mEnd;
            const bool ok = (ReadFlat<SUFFIX_MIN[Is + 1]>(cur, end, NthArg<OFFSET + Is>(values...)) && ...);
            if(ok) [[likely]]
                mCur = cur;
            return ok;
        }

        template <size_t BEGIN, typename... Ts>
        R0R_INLINE bool ReadRunsFrom(Ts&... values)
        {
            constexpr size_t COUNT = sizeof...(Ts);
            if constexpr(BEGIN == COUNT)
                return true;
            else
            {
                constexpr size_t RUN_END = FirstNonFlat<BEGIN, Ts...>();
                if constexpr(RUN_END > BEGIN)
                {
                    if(!ReadFlatRun<BEGIN>(std::make_index_sequence<RUN_END - BEGIN> {}, values...)) [[unlikely]]
                        return false;
                }

                if constexpr(RUN_END < COUNT)
                {
                    ReadValue(NthArg<RUN_END>(values...));
                    if(HasError()) [[unlikely]]
                        return false;
                    return ReadRunsFrom<RUN_END + 1>(values...);
                }
                else
                    return true;
            }
        }

        R0R_INLINE bool IsOverflowing(size_t size) noexcept
        {
            if(static_cast<size_t>(mEnd - mCur) < size) [[unlikely]]
            {
                SetError(ErrorCode::BUFFER_OVERFLOW);
                return true;
            }
            return false;
        }

    private:
        const Byte* mStart;
        const Byte* mCur;
        const Byte* mEnd;       // Read window end, equals mBufferEnd until an error closes the window
        const Byte* mBufferEnd; // True end of the input
        ErrorCode mStatus;
    };
}
