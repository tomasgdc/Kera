#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <optional>

namespace kera {

// Basic types
using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;

using i8 = int8_t;
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;

using f32 = float;
using f64 = double;

// SDL forward declarations
struct SDL_Window;

// Common data structures
struct Extent2D {
    u32 width = 0;
    u32 height = 0;

    bool operator==(const Extent2D& other) const {
        return width == other.width && height == other.height;
    }

    bool operator!=(const Extent2D& other) const {
        return !(*this == other);
    }
};

struct Offset2D {
    i32 x = 0;
    i32 y = 0;

    bool operator==(const Offset2D& other) const {
        return x == other.x && y == other.y;
    }

    bool operator!=(const Offset2D& other) const {
        return !(*this == other);
    }
};

struct Rect2D {
    Offset2D offset;
    Extent2D extent;

    bool operator==(const Rect2D& other) const {
        return offset == other.offset && extent == other.extent;
    }

    bool operator!=(const Rect2D& other) const {
        return !(*this == other);
    }
};

// Version structure
struct Version {
    u32 major = 0;
    u32 minor = 0;
    u32 patch = 0;

    bool operator==(const Version& other) const {
        return major == other.major && minor == other.minor && patch == other.patch;
    }

    bool operator!=(const Version& other) const {
        return !(*this == other);
    }

    bool operator<(const Version& other) const {
        if (major != other.major) return major < other.major;
        if (minor != other.minor) return minor < other.minor;
        return patch < other.patch;
    }

    std::string toString() const {
        return std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(patch);
    }
};

} // namespace kera
