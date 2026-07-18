// Copyright 2026 Tomas Mikalauskas
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

namespace kera
{

    template <typename T, typename HandleT>
    class ResourceRegistry
    {
    public:
        template <typename... Args>
        HandleT emplace(Args&&... args)
        {
            uint32_t slot_index = 0;
            if (!m_free_list.empty())
            {
                slot_index = m_free_list.back();
                m_free_list.pop_back();
            }
            else
            {
                m_slots.push_back({});
                slot_index = static_cast<uint32_t>(m_slots.size() - 1);
                m_slots[slot_index].m_generation = 1;
            }

            Slot& slot = m_slots[slot_index];
            slot.m_value.emplace(std::forward<Args>(args)...);
            return HandleT{static_cast<int32_t>(slot_index), slot.m_generation};
        }

        T* get(HandleT handle)
        {
            Slot* slot = getSlot(handle);
            return slot && slot->m_value ? &*slot->m_value : nullptr;
        }

        const T* get(HandleT handle) const
        {
            const Slot* slot = getSlot(handle);
            return slot && slot->m_value ? &*slot->m_value : nullptr;
        }

        bool remove(HandleT handle)
        {
            Slot* slot = getSlot(handle);
            if (!slot || !slot->m_value)
            {
                return false;
            }

            slot->m_value.reset();
            ++slot->m_generation;
            if (slot->m_generation == 0)
            {
                slot->m_generation = 1;
            }
            m_free_list.push_back(static_cast<uint32_t>(handle.m_index));
            return true;
        }

        std::optional<T> take(HandleT handle)
        {
            Slot* slot = getSlot(handle);
            if (!slot || !slot->m_value)
            {
                return std::nullopt;
            }

            std::optional<T> value{std::move(*slot->m_value)};
            slot->m_value.reset();
            ++slot->m_generation;
            if (slot->m_generation == 0)
            {
                slot->m_generation = 1;
            }
            m_free_list.push_back(static_cast<uint32_t>(handle.m_index));
            return value;
        }

        uint32_t activeCount() const
        {
            uint32_t count = 0;
            for (const Slot& slot : m_slots)
            {
                if (slot.m_value)
                {
                    ++count;
                }
            }
            return count;
        }

        void clear()
        {
            for (Slot& slot : m_slots)
            {
                if (slot.m_value)
                {
                    slot.m_value.reset();
                    ++slot.m_generation;
                    if (slot.m_generation == 0)
                    {
                        slot.m_generation = 1;
                    }
                }
            }
            m_free_list.clear();
            for (uint32_t i = 0; i < m_slots.size(); ++i)
            {
                m_free_list.push_back(i);
            }
        }

        template <typename Fn>
        bool forEach(Fn&& fn)
        {
            for (uint32_t i = 0; i < m_slots.size(); ++i)
            {
                Slot& slot = m_slots[i];
                if (slot.m_value)
                {
                    if (!fn(HandleT{static_cast<int32_t>(i), slot.m_generation}, *slot.m_value))
                    {
                        return false;
                    }
                }
            }
            return true;
        }

        template <typename Fn>
        bool forEach(Fn&& fn) const
        {
            for (uint32_t i = 0; i < m_slots.size(); ++i)
            {
                const Slot& slot = m_slots[i];
                if (slot.m_value)
                {
                    if (!fn(HandleT{static_cast<int32_t>(i), slot.m_generation}, *slot.m_value))
                    {
                        return false;
                    }
                }
            }
            return true;
        }

    private:
        struct Slot
        {
            uint32_t m_generation = 1;
            std::optional<T> m_value;
        };

        Slot* getSlot(HandleT handle)
        {
            if (!handle.isValid() || static_cast<uint32_t>(handle.m_index) >= m_slots.size())
            {
                return nullptr;
            }

            Slot& slot = m_slots[static_cast<uint32_t>(handle.m_index)];
            return slot.m_generation == handle.m_generation ? &slot : nullptr;
        }

        const Slot* getSlot(HandleT handle) const
        {
            if (!handle.isValid() || static_cast<uint32_t>(handle.m_index) >= m_slots.size())
            {
                return nullptr;
            }

            const Slot& slot = m_slots[static_cast<uint32_t>(handle.m_index)];
            return slot.m_generation == handle.m_generation ? &slot : nullptr;
        }

        std::vector<Slot> m_slots;
        std::vector<uint32_t> m_free_list;
    };

}  // namespace kera
