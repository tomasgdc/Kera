#pragma once

#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

namespace kera {

template <typename T, typename HandleT>
class ResourceRegistry {
 public:
  template <typename... Args>
  HandleT emplace(Args&&... args) {
    uint32_t slotIndex = 0;
    if (!m_freeList.empty()) {
      slotIndex = m_freeList.back();
      m_freeList.pop_back();
    } else {
      m_slots.push_back({});
      slotIndex = static_cast<uint32_t>(m_slots.size() - 1);
      m_slots[slotIndex].m_generation = 1;
    }

    Slot& slot = m_slots[slotIndex];
    slot.m_value.emplace(std::forward<Args>(args)...);
    return HandleT{static_cast<int32_t>(slotIndex), slot.m_generation};
  }

  T* get(HandleT handle) {
    Slot* slot = getSlot(handle);
    return slot && slot->m_value ? &*slot->m_value : nullptr;
  }

  const T* get(HandleT handle) const {
    const Slot* slot = getSlot(handle);
    return slot && slot->m_value ? &*slot->m_value : nullptr;
  }

  bool remove(HandleT handle) {
    Slot* slot = getSlot(handle);
    if (!slot || !slot->m_value) {
      return false;
    }

    slot->m_value.reset();
    ++slot->m_generation;
    if (slot->m_generation == 0) {
      slot->m_generation = 1;
    }
    m_freeList.push_back(static_cast<uint32_t>(handle.m_index));
    return true;
  }

  uint32_t activeCount() const {
    uint32_t count = 0;
    for (const Slot& slot : m_slots) {
      if (slot.m_value) {
        ++count;
      }
    }
    return count;
  }

  void clear() {
    for (Slot& slot : m_slots) {
      if (slot.m_value) {
        slot.m_value.reset();
        ++slot.m_generation;
        if (slot.m_generation == 0) {
          slot.m_generation = 1;
        }
      }
    }
    m_freeList.clear();
    for (uint32_t i = 0; i < m_slots.size(); ++i) {
      m_freeList.push_back(i);
    }
  }

  template <typename Fn>
  bool forEach(Fn&& fn) {
    for (uint32_t i = 0; i < m_slots.size(); ++i) {
      Slot& slot = m_slots[i];
      if (slot.m_value) {
        if (!fn(HandleT{static_cast<int32_t>(i), slot.m_generation},
                *slot.m_value)) {
          return false;
        }
      }
    }
    return true;
  }

 private:
  struct Slot {
    uint32_t m_generation = 1;
    std::optional<T> m_value;
  };

  Slot* getSlot(HandleT handle) {
    if (!handle.isValid() ||
        static_cast<uint32_t>(handle.m_index) >= m_slots.size()) {
      return nullptr;
    }

    Slot& slot = m_slots[static_cast<uint32_t>(handle.m_index)];
    return slot.m_generation == handle.m_generation ? &slot : nullptr;
  }

  const Slot* getSlot(HandleT handle) const {
    if (!handle.isValid() ||
        static_cast<uint32_t>(handle.m_index) >= m_slots.size()) {
      return nullptr;
    }

    const Slot& slot = m_slots[static_cast<uint32_t>(handle.m_index)];
    return slot.m_generation == handle.m_generation ? &slot : nullptr;
  }

  std::vector<Slot> m_slots;
  std::vector<uint32_t> m_freeList;
};

}  // namespace kera
