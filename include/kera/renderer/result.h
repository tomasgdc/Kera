#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace kera
{
    enum class RendererErrorCode : uint32_t
    {
        None = 0,
        InvalidHandle,
        InvalidState,
        Unsupported,
        OutOfMemory,
        DeviceLost,
        SwapchainOutOfDate,
        ReflectionMissing,
        ValidationFailed,
        ResourceInUse,
        BackendFailure,
    };

    enum class RendererValidationCategory : uint32_t
    {
        General = 0,
        Descriptor,
    };

    inline const char* rendererValidationCategoryName(RendererValidationCategory category) noexcept
    {
        switch (category)
        {
            case RendererValidationCategory::Descriptor:
                return "Descriptor";
            case RendererValidationCategory::General:
            default:
                return "General";
        }
    }

    struct RendererError
    {
        RendererErrorCode code = RendererErrorCode::None;
        std::string message;

        explicit operator bool() const noexcept
        {
            return code != RendererErrorCode::None || !message.empty();
        }
    };

    struct RendererValidationIssue
    {
        RendererErrorCode code = RendererErrorCode::ValidationFailed;
        RendererValidationCategory category = RendererValidationCategory::General;
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
            static const std::string emptyMessage;
            return issues.empty() ? emptyMessage : issues.front().message;
        }

        void addIssue(std::string message, uint32_t set = 0, uint32_t binding = 0, std::string name = {})
        {
            addIssue(RendererErrorCode::ValidationFailed, RendererValidationCategory::General, std::move(message), set,
                     binding, std::move(name));
        }

        void addIssue(RendererErrorCode code, std::string message, uint32_t set = 0, uint32_t binding = 0,
                      std::string name = {})
        {
            addIssue(code, RendererValidationCategory::General, std::move(message), set, binding, std::move(name));
        }

        void addIssue(RendererValidationCategory category, std::string message, uint32_t set = 0, uint32_t binding = 0,
                      std::string name = {})
        {
            addIssue(RendererErrorCode::ValidationFailed, category, std::move(message), set, binding, std::move(name));
        }

        void addIssue(RendererErrorCode code, RendererValidationCategory category, std::string message,
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
            return failure(RendererErrorCode::BackendFailure, std::move(message));
        }

        static RendererResult failure(RendererErrorCode code, std::string message)
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

        RendererErrorCode errorCode() const noexcept
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
            return failure(RendererErrorCode::BackendFailure, std::move(message));
        }

        static RendererResult failure(RendererErrorCode code, std::string message)
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

        RendererErrorCode errorCode() const noexcept
        {
            return m_error.code;
        }

    private:
        RendererError m_error{};
        bool m_ok = false;
    };

}  // namespace kera
