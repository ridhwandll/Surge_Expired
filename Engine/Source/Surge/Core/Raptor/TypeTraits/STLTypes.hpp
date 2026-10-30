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

  Rapt0R STL Container Extensions
  Supports: std::array, std::pair, std::tuple,
  std::optional, std::map, std::unordered_map,
  std::set, std::unordered_set, std::variant
*/

#pragma once
#include "../Defines.hpp"
#include "../Reader.hpp"
#include "../Writer.hpp"
#include "TypeTraits.hpp"
#include "Funcs.hpp"

#include <algorithm>
#include <array>
#include <map>
#include <optional>
#include <set>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>

namespace Rapt0R
{
    namespace Detail
    {
        // Smallest possible encoded size of one map entry, used to bound counts before allocating
        template <typename K, typename V>
        inline constexpr size_t MinPairBytes = (IsRawCopyable_V<K> && IsRawCopyable_V<V>) ? sizeof(K) + sizeof(V) : 1;

        template <typename K>
        inline constexpr size_t MinElementBytes = IsRawCopyable_V<K> ? sizeof(K) : 1;

        // Shared by maps and sets: count + entries (map entries are key then value). When every key/value is flat,
        // small containers are written under a single capacity check.
        template <typename Container>
        void SerializeAssociative(Writer& w, const Container& container) noexcept
        {
            using E = typename Container::value_type;
            constexpr bool FLAT = [] {
                if constexpr(IsStdPair_V<E>)
                    return WriterFlatKind_V<typename E::first_type> != FlatKind::NONE && WriterFlatKind_V<typename E::second_type> != FlatKind::NONE;
                else
                    return WriterFlatKind_V<E> != FlatKind::NONE;
            }();

            if constexpr(FLAT)
                w.WriteFlatContainer(container);
            else
            {
                if(!w.ValidateCount(container.size())) [[unlikely]]
                    return;

                w.Write(static_cast<uint32_t>(container.size()));
                for(const auto& element : container)
                {
                    if constexpr(IsStdPair_V<E>)
                    {
                        if(!w.Write(element.first, element.second)) [[unlikely]]
                            return;
                    }
                    else if(!w.Write(element)) [[unlikely]]
                        return;
                }
            }
        }

        template <typename K, typename V>
        R0R_INLINE bool ReadMapEntry(Reader& r, K& key, V& val)
        {
            return r.Read(key, val); // Fused: single check per K/V pair (plus one per string payload)
        }

        // Fills a node-based container (std::map, std::set and their unordered variants) with `count` entries while
        // recycling the nodes it already owns: they are extracted, overwritten in place and reinserted. Reloading into a
        // container of similar shape therefore reuses node, key and value allocations instead of freeing and reallocating
        // them, and hash containers keep their bucket array.
        template <typename Container, typename ReadIntoNode, typename ReadFresh>
        void DeserializeRecyclingNodes(Container& container, uint32_t count, ReadIntoNode&& readIntoNode, ReadFresh&& readFresh)
        {
            using Node = typename Container::node_type;
            constexpr size_t STACK_NODES = 16;

            if constexpr(requires { container.reserve(count); })
                container.reserve(count);

            // Fresh container, nothing to recycle: skip the node handle machinery entirely
            if(container.empty())
            {
                for(uint32_t i = 0; i < count; ++i)
                {
                    if(!readFresh()) [[unlikely]]
                        return;
                }
                return;
            }

            const size_t recycleCount = std::min<size_t>(container.size(), count);
            Node stackNodes[STACK_NODES];
            std::vector<Node> heapNodes;
            Node* nodes = stackNodes;
            if(recycleCount > STACK_NODES)
            {
                heapNodes.resize(recycleCount);
                nodes = heapNodes.data();
            }

            for(size_t i = 0; i < recycleCount; ++i)
                nodes[i] = container.extract(container.begin());
            container.clear(); // Only frees the surplus nodes we are not going to reuse

            for(uint32_t i = 0; i < count; ++i)
            {
                if(i < recycleCount)
                {
                    Node& node = nodes[i];
                    if(!readIntoNode(node)) [[unlikely]]
                        return;
                    // Entries are serialized in iteration order, so the end() hint is exact for ordered containers.
                    // On a duplicate key the node stays in `nodes` and is released when we return.
                    container.insert(container.end(), std::move(node));
                }
                else if(!readFresh()) [[unlikely]]
                    return;
            }
        }
    }

    // ========================================================================
    // 1. std::array<T, N> (For Non-POD elements like std::array<std::string, 4>)
    //    Note: POD std::array<float, N> automatically takes the fast memcpy path!
    // ========================================================================
    template <typename T, size_t N> requires (!IsRawCopyable_V<T>)
    R0R_INLINE void RAPTOR_SERIALIZE_FUNC(Writer& w, const std::array<T, N>& arr) noexcept
    {
        for(const auto& item : arr)
            w.Write(item);
    }

    template <typename T, size_t N> requires (!IsRawCopyable_V<T>)
    R0R_INLINE void RAPTOR_DESERIALIZE_FUNC(Reader& r, std::array<T, N>& arr)
    {
        for(auto& item : arr)
            r.Read(item);
    }

    // ========================================================================
    // 2. std::pair<K, V> (Fused Write/Read: 1 bounds check, zero struct-padding waste!)
    // ========================================================================
    template <typename K, typename V>
    R0R_INLINE void RAPTOR_SERIALIZE_FUNC(Writer& w, const std::pair<K, V>& p) noexcept
    {
        w.Write(p.first, p.second);
    }

    template <typename K, typename V>
    R0R_INLINE void RAPTOR_DESERIALIZE_FUNC(Reader& r, std::pair<K, V>& p)
    {
        r.Read(p.first, p.second);
    }

    // ========================================================================
    // 3. std::tuple<Ts...> (C++20 std::apply + fused Write/Read)
    // ========================================================================
    template <typename... Ts>
    R0R_INLINE void RAPTOR_SERIALIZE_FUNC(Writer& w, const std::tuple<Ts...>& tup) noexcept
    {
        if constexpr(sizeof...(Ts) > 0)
            std::apply([&w](const Ts&... args) { w.Write(args...); }, tup);
    }

    template <typename... Ts>
    R0R_INLINE void RAPTOR_DESERIALIZE_FUNC(Reader& r, std::tuple<Ts...>& tup)
    {
        if constexpr(sizeof...(Ts) > 0)
            std::apply([&r](Ts&... args) { r.Read(args...); }, tup);
    }

    // ========================================================================
    // 4. std::optional<T> (1-Byte Bool Header + Conditional Value)
    // ========================================================================
    template <typename T>
    R0R_INLINE void RAPTOR_SERIALIZE_FUNC(Writer& w, const std::optional<T>& opt) noexcept
    {
        const uint8_t hasVal = opt.has_value() ? 1 : 0;
        if constexpr(IsRawCopyable_V<T>)
        {
            if(hasVal)
                w.WritePODs(hasVal, *opt); // Merged 1-check write for flag + POD payload!
            else
                w.Write(hasVal);
        }
        else
        {
            w.Write(hasVal);
            if(hasVal)
                w.Write(*opt);
        }
    }

    template <typename T>
    R0R_INLINE void RAPTOR_DESERIALIZE_FUNC(Reader& r, std::optional<T>& opt)
    {
        uint8_t hasVal = 0;
        r.Read(hasVal);
        if(r.HasError()) [[unlikely]]
            return;

        if(hasVal > 1) [[unlikely]]
        {
            r.SetError(ErrorCode::CORRUPT_DATA);
            return;
        }

        if(hasVal)
        {
            if(!opt.has_value())
                opt.emplace(); // Construct in-place (reuses allocation if already engaged!)
            r.Read(*opt);
        }
        else
        {
            opt.reset();
        }
    }

    // ========================================================================
    // 5. Associative Maps: std::unordered_map & std::map
    // ========================================================================
    template <typename K, typename V, typename Hash, typename Eq, typename Alloc>
    inline void RAPTOR_SERIALIZE_FUNC(Writer& w, const std::unordered_map<K, V, Hash, Eq, Alloc>& map) noexcept
    {
        Detail::SerializeAssociative(w, map);
    }

    template <typename K, typename V, typename Hash, typename Eq, typename Alloc>
    inline void RAPTOR_DESERIALIZE_FUNC(Reader& r, std::unordered_map<K, V, Hash, Eq, Alloc>& map)
    {
        uint32_t count = 0;
        if(!r.Read(count)) [[unlikely]]
            return;

        // Bound the count by the remaining input BEFORE reserving, a hostile count must not trigger a huge allocation
        if(!r.EnsureElements(count, Detail::MinPairBytes<K, V>)) [[unlikely]]
            return;

        // Reserves the hash buckets once to prevent rehashing!
        Detail::DeserializeRecyclingNodes(map, count,
            [&r](auto& node) { return Detail::ReadMapEntry(r, node.key(), node.mapped()); },
            [&r, &map]() {
                K key {};
                V val {};
                if(!Detail::ReadMapEntry(r, key, val)) [[unlikely]]
                    return false;
                map.emplace(std::move(key), std::move(val));
                return true;
            });
    }

    template <typename K, typename V, typename Cmp, typename Alloc>
    inline void RAPTOR_SERIALIZE_FUNC(Writer& w, const std::map<K, V, Cmp, Alloc>& map) noexcept
    {
        Detail::SerializeAssociative(w, map);
    }

    template <typename K, typename V, typename Cmp, typename Alloc>
    inline void RAPTOR_DESERIALIZE_FUNC(Reader& r, std::map<K, V, Cmp, Alloc>& map)
    {
        uint32_t count = 0;
        if(!r.Read(count)) [[unlikely]]
            return;

        if(!r.EnsureElements(count, Detail::MinPairBytes<K, V>)) [[unlikely]]
            return;

        Detail::DeserializeRecyclingNodes(map, count,
            [&r](auto& node) { return Detail::ReadMapEntry(r, node.key(), node.mapped()); },
            [&r, &map]() {
                K key {};
                V val {};
                if(!Detail::ReadMapEntry(r, key, val)) [[unlikely]]
                    return false;
                // Since std::map was serialized in sorted order, emplace_hint(map.end(), ...)
                // inserts in O(1) amortized time instead of O(log N) tree search!
                map.emplace_hint(map.end(), std::move(key), std::move(val));
                return true;
            });
    }

    // ========================================================================
    // 6. Sets: std::unordered_set & std::set
    // ========================================================================
    template <typename K, typename Hash, typename Eq, typename Alloc>
    inline void RAPTOR_SERIALIZE_FUNC(Writer& w, const std::unordered_set<K, Hash, Eq, Alloc>& set) noexcept
    {
        Detail::SerializeAssociative(w, set);
    }

    template <typename K, typename Hash, typename Eq, typename Alloc>
    inline void RAPTOR_DESERIALIZE_FUNC(Reader& r, std::unordered_set<K, Hash, Eq, Alloc>& set)
    {
        uint32_t count = 0;
        if(!r.Read(count)) [[unlikely]]
            return;

        if(!r.EnsureElements(count, Detail::MinElementBytes<K>)) [[unlikely]]
            return;

        Detail::DeserializeRecyclingNodes(set, count,
            [&r](auto& node) { return r.Read(node.value()); },
            [&r, &set]() {
                K item {};
                if(!r.Read(item)) [[unlikely]]
                    return false;
                set.emplace(std::move(item));
                return true;
            });
    }

    template <typename K, typename Cmp, typename Alloc>
    inline void RAPTOR_SERIALIZE_FUNC(Writer& w, const std::set<K, Cmp, Alloc>& set) noexcept
    {
        Detail::SerializeAssociative(w, set);
    }

    template <typename K, typename Cmp, typename Alloc>
    inline void RAPTOR_DESERIALIZE_FUNC(Reader& r, std::set<K, Cmp, Alloc>& set)
    {
        uint32_t count = 0;
        if(!r.Read(count)) [[unlikely]]
            return;

        if(!r.EnsureElements(count, Detail::MinElementBytes<K>)) [[unlikely]]
            return;

        Detail::DeserializeRecyclingNodes(set, count,
            [&r](auto& node) { return r.Read(node.value()); },
            [&r, &set]() {
                K item {};
                if(!r.Read(item)) [[unlikely]]
                    return false;
                // Serialized in sorted order, so appending at end() is amortized O(1)
                set.emplace_hint(set.end(), std::move(item));
                return true;
            });
    }

    // ========================================================================
    // 7. std::variant<Ts...> (1-Byte Alternative Index + Active Alternative)
    // ========================================================================
    namespace Detail
    {
        template <size_t I, typename Variant>
        R0R_INLINE void DeserializeVariantAlternative(Reader& r, Variant& var)
        {
            if(var.index() != I)
                var.template emplace<I>(); // Keep the existing alternative (and its allocations) if it already matches
            r.Read(std::get<I>(var));
        }
    }

    template <typename... Ts>
    inline void RAPTOR_SERIALIZE_FUNC(Writer& w, const std::variant<Ts...>& var) noexcept
    {
        static_assert(sizeof...(Ts) <= UINT8_MAX, "Rapt0R encodes the variant index as a single byte");

        if(var.valueless_by_exception()) [[unlikely]]
        {
            w.SetError(ErrorCode::FAILED);
            return;
        }

        w.Write(static_cast<uint8_t>(var.index()));
        std::visit([&w](const auto& alternative) { w.Write(alternative); }, var);
    }

    template <typename... Ts>
    inline void RAPTOR_DESERIALIZE_FUNC(Reader& r, std::variant<Ts...>& var)
    {
        uint8_t index = 0;
        if(!r.Read(index)) [[unlikely]]
            return;

        if(index >= sizeof...(Ts)) [[unlikely]]
        {
            r.SetError(ErrorCode::CORRUPT_DATA);
            return;
        }

        [&]<size_t... Is>(std::index_sequence<Is...>) {
            (void)((index == Is && (Detail::DeserializeVariantAlternative<Is>(r, var), true)) || ...);
        }(std::index_sequence_for<Ts...> {});
    }
}
