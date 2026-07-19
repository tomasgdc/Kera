// Copyright 2026 Tomas Mikalauskas
// SPDX-License-Identifier: Apache-2.0

#include "kera/renderer/api.h"

#include <gtest/gtest.h>

#include <cstddef>
#include <type_traits>

namespace
{
#if defined(_CPPUNWIND) || defined(__EXCEPTIONS)
#error "Kera tests must compile with C++ exceptions disabled."
#endif

#if defined(_MSC_VER) && defined(_HAS_EXCEPTIONS) && (_HAS_EXCEPTIONS == 0)
    static_assert(_HAS_EXCEPTIONS == 0, "Kera MSVC builds must compile the STL with exceptions disabled.");
#endif

    static_assert(std::is_standard_layout_v<KeraStringView>);
    static_assert(std::is_standard_layout_v<KeraByteView>);
    static_assert(std::is_standard_layout_v<KeraHandle>);
    static_assert(std::is_standard_layout_v<KeraRendererApiV1>);
    static_assert(sizeof(KeraStringView) == sizeof(const char*) + sizeof(size_t));
    static_assert(KERA_RENDERER_ABI_VERSION == 1u);

    struct PublicVertex
    {
        float position[3];
        float color[3];
    };
}  // namespace

TEST(KeraRendererPublicApi, BufferDescriptorsAndFunctionTableAreAbiStable)
{
    const KeraStringView debug_name{
        "Public Buffer",
        sizeof("Public Buffer") - 1,
    };

    const KeraBufferDesc buffer_desc{
        256,
        KERA_BUFFER_USAGE_UNIFORM,
        KERA_MEMORY_ACCESS_CPU_WRITE,
        debug_name,
    };
    EXPECT_EQ(buffer_desc.size, 256u);
    EXPECT_EQ(buffer_desc.debug_name.data, debug_name.data);
    EXPECT_EQ(buffer_desc.debug_name.size, debug_name.size);

    KeraRendererApiV1 api{};
    api.abi_version = KERA_RENDERER_ABI_VERSION;
    EXPECT_EQ(api.abi_version, 1u);
    ASSERT_NE(keraGetRendererApiV1(), nullptr);
    EXPECT_EQ(keraGetRendererApiV1()->abi_version, KERA_RENDERER_ABI_VERSION);
    EXPECT_NE(keraGetRendererApiV1()->validate_vertex_input_layout, nullptr);
}

TEST(KeraRendererPublicApi, VertexInputLayoutBuilderOnlyPackagesFields)
{
    const kera::VertexInputLayout layout =
        kera::VertexInputLayoutBuilder{}
            .vertexBinding<PublicVertex>(0)
            .field(KERA_VERTEX_FIELD(PublicVertex, position, 0, KERA_VERTEX_FORMAT_FLOAT3))
            .field(KERA_VERTEX_FIELD(PublicVertex, color, 0, KERA_VERTEX_FORMAT_FLOAT3))
            .layout();

    ASSERT_EQ(layout.binding_count, 1u);
    EXPECT_EQ(layout.bindings[0].binding, 0u);
    EXPECT_EQ(layout.bindings[0].stride, sizeof(PublicVertex));
    ASSERT_EQ(layout.field_count, 2u);
    EXPECT_EQ(layout.fields[0].field_name.size, sizeof("position") - 1u);
    EXPECT_EQ(layout.fields[0].binding, 0u);
    EXPECT_EQ(layout.fields[1].offset, offsetof(PublicVertex, color));
}
