#pragma once

#include "kera/renderer/descriptors.h"

namespace kera
{

    inline const DescriptorBindingDesc* findDescriptorBinding(const DescriptorSetLayoutDesc& layout, uint32_t binding)
    {
        for (const DescriptorBindingDesc& bindingDesc : layout.bindings)
        {
            if (bindingDesc.binding == binding)
            {
                return &bindingDesc;
            }
        }
        return nullptr;
    }

    inline bool descriptorBindingAccepts(const DescriptorSetLayoutDesc& layout, uint32_t binding, DescriptorType type,
                                         uint32_t descriptorCount = 1)
    {
        const DescriptorBindingDesc* bindingDesc = findDescriptorBinding(layout, binding);
        return bindingDesc && bindingDesc->type == type && bindingDesc->count == descriptorCount;
    }

    inline bool descriptorSetLayoutsCompatible(const DescriptorSetLayoutDesc& lhs, const DescriptorSetLayoutDesc& rhs)
    {
        if (lhs.set != rhs.set || lhs.bindings.size() != rhs.bindings.size())
        {
            return false;
        }

        for (const DescriptorBindingDesc& lhsBinding : lhs.bindings)
        {
            const DescriptorBindingDesc* rhsBinding = findDescriptorBinding(rhs, lhsBinding.binding);
            if (!rhsBinding || rhsBinding->type != lhsBinding.type || rhsBinding->stage != lhsBinding.stage ||
                rhsBinding->count != lhsBinding.count)
            {
                return false;
            }
        }

        return true;
    }

}  // namespace kera
