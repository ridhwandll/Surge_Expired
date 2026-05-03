// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Defines.hpp"

namespace Surge
{
	template<typename Tag>
	struct RHIHandle
	{
		static constexpr Uint INVALID_INDEX = ~0u;

		Uint Index = INVALID_INDEX;
		Uint Generation = 0;

		bool IsNull() const { return Index == INVALID_INDEX; }

		bool operator==(const RHIHandle& o) const { return Index == o.Index && Generation == o.Generation; }
		bool operator!=(const RHIHandle& o) const { return !(*this == o); }

		static RHIHandle Invalid() { return {}; } // Index = ~0u, Generation = 0
	};

	struct FramebufferTag {};
	using FramebufferHandle = RHIHandle<FramebufferTag>;

	struct TextureTag {};
	using TextureHandle = RHIHandle<TextureTag>;

	struct BufferTag {};
	using BufferHandle = RHIHandle<BufferTag>;

	struct PipelineTag {};
	using PipelineHandle = RHIHandle<PipelineTag>;

	template<typename XHandle, typename T>
	class HandlePool
	{
	public:
		struct Slot
		{
			T Data = {};
			Uint Generation = 0;
			bool Alive = false;
		};

		// Allocate a slot and move data into it. Returns a valid handle.
		XHandle Allocate(T&& value)
		{
			Uint index;

			if (!mFreeList.empty())
			{
				index = mFreeList.back();
				mFreeList.pop_back();
			}
			else
			{
				index = static_cast<Uint>(mSlots.size());
				mSlots.emplace_back();
			}

			Slot& slot = mSlots[index];
			slot.Data = std::move(value);
			slot.Alive = true;

			// Guard against generation wrapping back to 0
			// (a stale handle with Generation=N would match a recycled slot
			//  if the slot wrapped around to the same generation)
			slot.Generation = (slot.Generation == ~0u) ? 1 : slot.Generation + 1;

			XHandle handle;
			handle.Index = index;
			handle.Generation = slot.Generation;
			return handle;
		}

		// Returns true only if the handle was issued by this pool
		// and the slot has not been destroyed since.
		bool IsValid(XHandle h) const
		{
			if (h.IsNull() || h.Index >= static_cast<Uint>(mSlots.size()))
				return false;

			const Slot& s = mSlots[h.Index];
			return s.Alive && s.Generation == h.Generation;
		}

		// Get mutable data. Returns nullptr for invalid/stale handles.
		T* Get(XHandle h)
		{
			if (!IsValid(h))
				return nullptr;

			return &mSlots[h.Index].Data;
		}

		// Get immutable data.
		const T* Get(XHandle h) const
		{
			if (!IsValid(h)) return nullptr;
			return &mSlots[h.Index].Data;
		}

		// Mark the slot as free and reset its data.
		// IMPORTANT: destroy any GPU resources BEFORE calling this.
		// After Destroy() returns, the data is gone.
		void Free(XHandle h)
		{
			if (!IsValid(h))
				return;

			Slot& s = mSlots[h.Index];
			s.Data = T{};
			s.Alive = false;
			mFreeList.push_back(h.Index);
		}

		// Iterate all currently live slots. Callback: void(XHandle, T&)
		template<typename Fn>
		void ForEachAlive(Fn&& fn)
		{
			for (Uint i = 0; i < static_cast<Uint>(mSlots.size()); i++)
			{
				Slot& s = mSlots[i];

				if (!s.Alive)
					continue;

				XHandle h;
				h.Index = i;
				h.Generation = s.Generation;
				fn(h, s.Data);
			}
		}

		Uint AliveObjCount() const
		{
			Uint count = 0;
			for (const auto& s : mSlots)
				count += s.Alive ? 1 : 0;

			return count;
		}

	private:
		Vector<Slot> mSlots;
		Vector<Uint> mFreeList;
	};

} // namespace Surge