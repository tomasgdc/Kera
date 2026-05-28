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

#if defined(_MSC_VER)
    static_assert(_HAS_EXCEPTIONS == 0, "Kera MSVC builds must compile the STL with exceptions disabled.");
#endif

    static_assert(std::is_standard_layout_v<KeraStringView>);
    static_assert(std::is_standard_layout_v<KeraByteView>);
    static_assert(std::is_standard_layout_v<KeraHandle>);
    static_assert(std::is_standard_layout_v<KeraRendererApiV1>);
    static_assert(sizeof(KeraStringView) == sizeof(const char*) + sizeof(size_t));
    static_assert(KERA_RENDERER_ABI_VERSION == 1u);
}  // namespace

TEST(KeraRendererPublicApi, BufferDescriptorsAndFunctionTableAreAbiStable)
{
    const KeraStringView debugName{
        "Public Buffer",
        sizeof("Public Buffer") - 1,
    };

    const KeraBufferDesc bufferDesc{
        256,
        KERA_BUFFER_USAGE_UNIFORM,
        KERA_MEMORY_ACCESS_CPU_WRITE,
        debugName,
    };
    EXPECT_EQ(bufferDesc.size, 256u);
    EXPECT_EQ(bufferDesc.debugName.data, debugName.data);
    EXPECT_EQ(bufferDesc.debugName.size, debugName.size);

    KeraRendererApiV1 api{};
    api.abiVersion = KERA_RENDERER_ABI_VERSION;
    EXPECT_EQ(api.abiVersion, 1u);
    ASSERT_NE(keraGetRendererApiV1(), nullptr);
    EXPECT_EQ(keraGetRendererApiV1()->abiVersion, KERA_RENDERER_ABI_VERSION);
}
