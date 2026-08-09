// Copyright 2026 Tomas Mikalauskas
// SPDX-License-Identifier: Apache-2.0

#pragma once

// Private sample/test bridge. This header is deliberately not installed and is not part of a public API table.

#include "kera/renderer/abi.h"

#include <cstdint>
#include <string>
#include <vector>

namespace kera::test
{
    struct AttachmentCapture
    {
        uint32_t width = 0;
        uint32_t height = 0;
        KeraTextureFormat format = KERA_TEXTURE_FORMAT_RGBA8;
        std::vector<uint8_t> bytes;
    };

    KERA_API bool requestAttachmentCapture(KeraRenderer* renderer, KeraFrameHandle frame, KeraTextureHandle texture,
                                           const std::string& name) noexcept;
    KERA_API bool takeAttachmentCapture(KeraRenderer* renderer, const std::string& name, AttachmentCapture& capture,
                                        bool wait_for_completion = true) noexcept;
}  // namespace kera::test
