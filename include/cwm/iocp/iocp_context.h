#pragma once

#include <Windows.h>

#include <cstdint>

#include <cwm/platform/win32_handle.h>

namespace cwm::iocp
{

class IocpContext final
{
public:
    explicit IocpContext(
        DWORD concurrency = 0);

    ~IocpContext() noexcept = default;

    IocpContext(const IocpContext&) = delete;
    IocpContext& operator=(const IocpContext&) = delete;

    IocpContext(IocpContext&&) noexcept = default;
    IocpContext& operator=(IocpContext&&) noexcept = default;

    void associate(
        HANDLE handle,
        ULONG_PTR completion_key);

    void post(
        DWORD bytes_transferred,
        ULONG_PTR completion_key,
        OVERLAPPED* overlapped);

    [[nodiscard]]
    HANDLE native_handle() const noexcept;

private:
    cwm::platform::Win32Handle handle_;
};

} // namespace cwm::iocp
