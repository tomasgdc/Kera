#pragma once

// Core includes
#include "kera/core/window.h"
#include "kera/core/input.h"
#include "kera/core/audio.h"

// Renderer includes
#include "kera/renderer/instance.h"
#include "kera/renderer/physical_device.h"
#include "kera/renderer/device.h"
#include "kera/renderer/surface.h"
#include "kera/renderer/swapchain.h"
#include "kera/renderer/command_buffer.h"
#include "kera/renderer/pipeline.h"
#include "kera/renderer/render_pass.h"
#include "kera/renderer/framebuffer.h"
#include "kera/renderer/shader.h"
#include "kera/renderer/slang_compiler.h"
#include "kera/renderer/buffer.h"
#include "kera/renderer/image.h"

// Utilities includes
#include "kera/utilities/logger.h"
#include "kera/utilities/validation.h"
#include "kera/utilities/file_utils.h"

// Common includes
#include "kera/types.h"
#include "kera/result.h"

// Standard library includes commonly used
#include <memory>
#include <vector>
#include <string>
#include <optional>
#include <functional>
