#pragma once

#include <Windows.h>

namespace cwm::platform
{

class Win32Handle final
{
public:
    Win32Handle() noexcept = default;

    explicit Win32Handle(HANDLE handle) noexcept;

    ~Win32Handle() noexcept;

    Win32Handle(const Win32Handle&) = delete;
    Win32Handle& operator=(const Win32Handle&) = delete;

    Win32Handle(Win32Handle&& other) noexcept;
    Win32Handle& operator=(Win32Handle&& other) noexcept;

    [[nodiscard]]
    bool valid() const noexcept;

    [[nodiscard]]
    HANDLE get() const noexcept;

    [[nodiscard]]
    HANDLE release() noexcept;

    void reset(HANDLE handle = nullptr) noexcept;

private:
    HANDLE handle_{nullptr};
};

} // namespace cwm::platform

