#pragma once

#include <Windows.h>

#include <chrono>
#include <cstdint>
#include <optional>

#include <cwm/iocp/iocp_completion.h>
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
	std::optional<IocpCompletion> wait(std::chrono::milliseconds timeout);

    [[nodiscard]]
    HANDLE native_handle() const noexcept;

private:
    cwm::platform::Win32Handle handle_;
};

} // namespace cwm::iocp
