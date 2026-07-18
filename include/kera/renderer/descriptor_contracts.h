// Copyright 2026 Tomas Mikalauskas
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "kera/renderer/descriptors.h"

namespace kera
{

    inline const DescriptorBindingDesc* findDescriptorBinding(const DescriptorSetLayoutDesc& layout, uint32_t binding)
    {
        for (const DescriptorBindingDesc& binding_desc : layout.bindings)
        {
            if (binding_desc.binding == binding)
            {
                return &binding_desc;
            }
        }
        return nullptr;
    }

    inline const DescriptorBindingDesc* findDescriptorBinding(const DescriptorSetLayoutDesc& layout,
                                                              const std::string& name)
    {
        for (const DescriptorBindingDesc& binding_desc : layout.bindings)
        {
            if (binding_desc.name == name)
            {
                return &binding_desc;
            }
        }
        return nullptr;
    }

    inline bool descriptorBindingAccepts(const DescriptorSetLayoutDesc& layout, uint32_t binding, EDescriptorType type,
                                         uint32_t descriptor_count = 1)
    {
        const DescriptorBindingDesc* binding_desc = findDescriptorBinding(layout, binding);
        return binding_desc && binding_desc->type == type && binding_desc->count == descriptor_count;
    }

    inline bool descriptorSetLayoutsCompatible(const DescriptorSetLayoutDesc& lhs, const DescriptorSetLayoutDesc& rhs)
    {
        if (lhs.set != rhs.set || lhs.bindings.size() != rhs.bindings.size())
        {
            return false;
        }

        for (const DescriptorBindingDesc& lhs_binding : lhs.bindings)
        {
            const DescriptorBindingDesc* rhs_binding = findDescriptorBinding(rhs, lhs_binding.binding);
            if (!rhs_binding || rhs_binding->type != lhs_binding.type || rhs_binding->stage != lhs_binding.stage ||
                rhs_binding->count != lhs_binding.count)
            {
                return false;
            }
        }

        return true;
    }

}  // namespace kera
