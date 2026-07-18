// Copyright 2026 Tomas Mikalauskas
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace kera
{
    enum class ERendererErrorCode : uint32_t
    {
        NONE = 0,
        INVALID_HANDLE,
        INVALID_STATE,
        UNSUPPORTED,
        OUT_OF_MEMORY,
        DEVICE_LOST,
        SWAPCHAIN_OUT_OF_DATE,
        REFLECTION_MISSING,
        VALIDATION_FAILED,
        RESOURCE_IN_USE,
        BACKEND_FAILURE,
    };

    enum class ERendererValidationCategory : uint32_t
    {
        GENERAL = 0,
        DESCRIPTOR,
    };

    inline const char* rendererValidationCategoryName(ERendererValidationCategory category) noexcept
    {
        switch (category)
        {
            case ERendererValidationCategory::DESCRIPTOR:
                return "Descriptor";
            case ERendererValidationCategory::GENERAL:
            default:
                return "General";
        }
    }

    struct RendererError
    {
        ERendererErrorCode code = ERendererErrorCode::NONE;
        std::string message;

        explicit operator bool() const noexcept
        {
            return code != ERendererErrorCode::NONE || !message.empty();
        }
    };

    struct RendererValidationIssue
    {
        ERendererErrorCode code = ERendererErrorCode::VALIDATION_FAILED;
        ERendererValidationCategory category = ERendererValidationCategory::GENERAL;
        std::string message;
        std::string name;
        uint32_t set = 0;
        uint32_t binding = 0;
    };

    struct RendererValidationReport
    {
        std::vector<RendererValidationIssue> issues;

        bool ok() const noexcept
        {
            return issues.empty();
        }

        explicit operator bool() const noexcept
        {
            return ok();
        }

        const std::string& errorMessage() const noexcept
        {
            static const std::string empty_message;
            return issues.empty() ? empty_message : issues.front().message;
        }

        void addIssue(std::string message, uint32_t set = 0, uint32_t binding = 0, std::string name = {})
        {
            addIssue(ERendererErrorCode::VALIDATION_FAILED, ERendererValidationCategory::GENERAL, std::move(message),
                     set, binding, std::move(name));
        }

        void addIssue(ERendererErrorCode code, std::string message, uint32_t set = 0, uint32_t binding = 0,
                      std::string name = {})
        {
            addIssue(code, ERendererValidationCategory::GENERAL, std::move(message), set, binding, std::move(name));
        }

        void addIssue(ERendererValidationCategory category, std::string message, uint32_t set = 0, uint32_t binding = 0,
                      std::string name = {})
        {
            addIssue(ERendererErrorCode::VALIDATION_FAILED, category, std::move(message), set, binding,
                     std::move(name));
        }

        void addIssue(ERendererErrorCode code, ERendererValidationCategory category, std::string message,
                      uint32_t set = 0, uint32_t binding = 0, std::string name = {})
        {
            issues.push_back({
                .code = code,
                .category = category,
                .message = std::move(message),
                .name = std::move(name),
                .set = set,
                .binding = binding,
            });
        }
    };

    template <typename T>
    class RendererResult
    {
    public:
        static RendererResult success(T value)
        {
            RendererResult result;
            result.m_value = std::move(value);
            result.m_ok = true;
            return result;
        }

        static RendererResult failure(std::string message)
        {
            return failure(ERendererErrorCode::BACKEND_FAILURE, std::move(message));
        }

        static RendererResult failure(ERendererErrorCode code, std::string message)
        {
            RendererResult result;
            result.m_error.code = code;
            result.m_error.message = std::move(message);
            return result;
        }

        static RendererResult failure(RendererError error)
        {
            RendererResult result;
            result.m_error = std::move(error);
            return result;
        }

        bool ok() const noexcept
        {
            return m_ok;
        }

        explicit operator bool() const noexcept
        {
            return ok();
        }

        const T& value() const noexcept
        {
            return m_value;
        }

        T& value() noexcept
        {
            return m_value;
        }

        const T* valuePtr() const noexcept
        {
            return m_ok ? &m_value : nullptr;
        }

        T* valuePtr() noexcept
        {
            return m_ok ? &m_value : nullptr;
        }

        T valueOr(T fallback) const
        {
            return m_ok ? m_value : std::move(fallback);
        }

        const RendererError& error() const noexcept
        {
            return m_error;
        }

        const std::string& errorMessage() const noexcept
        {
            return m_error.message;
        }

        ERendererErrorCode errorCode() const noexcept
        {
            return m_error.code;
        }

    private:
        T m_value{};
        RendererError m_error{};
        bool m_ok = false;
    };

    template <>
    class RendererResult<void>
    {
    public:
        static RendererResult success()
        {
            RendererResult result;
            result.m_ok = true;
            return result;
        }

        static RendererResult failure(std::string message)
        {
            return failure(ERendererErrorCode::BACKEND_FAILURE, std::move(message));
        }

        static RendererResult failure(ERendererErrorCode code, std::string message)
        {
            RendererResult result;
            result.m_error.code = code;
            result.m_error.message = std::move(message);
            return result;
        }

        static RendererResult failure(RendererError error)
        {
            RendererResult result;
            result.m_error = std::move(error);
            return result;
        }

        bool ok() const noexcept
        {
            return m_ok;
        }

        explicit operator bool() const noexcept
        {
            return ok();
        }

        const RendererError& error() const noexcept
        {
            return m_error;
        }

        const std::string& errorMessage() const noexcept
        {
            return m_error.message;
        }

        ERendererErrorCode errorCode() const noexcept
        {
            return m_error.code;
        }

    private:
        RendererError m_error{};
        bool m_ok = false;
    };

}  // namespace kera
