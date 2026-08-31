#include <algorithm>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <system_error>

#include <cwm/iocp/iocp_context.h>
#include <cwm/iocp/iocp_operation.h>

namespace cwm::iocp
{

IocpContext::IocpContext(DWORD concurrency)
{
    HANDLE handle = ::CreateIoCompletionPort(
        INVALID_HANDLE_VALUE,
        nullptr,
        0,
        concurrency);

    if (handle == nullptr)
    {
        const DWORD error = ::GetLastError();

        throw std::system_error(
            static_cast<int>(error),
            std::system_category(),
            "CreateIoCompletionPort failed");
    }

    handle_.reset(handle);
}

void IocpContext::associate(
    HANDLE handle,
    ULONG_PTR completion_key)
{
    if (handle == nullptr)
    {
        throw std::invalid_argument(
            "IocpContext::associate received a null handle");
    }

    HANDLE result = ::CreateIoCompletionPort(
        handle,
        handle_.get(),
        completion_key,
        0);

    if (result == nullptr)
    {
        const DWORD error = ::GetLastError();

        throw std::system_error(
            static_cast<int>(error),
            std::system_category(),
            "CreateIoCompletionPort association failed");
    }
}

void IocpContext::post(
    DWORD bytes_transferred,
    ULONG_PTR completion_key,
    OVERLAPPED* overlapped)
{
    if (!::PostQueuedCompletionStatus(
            handle_.get(),
            bytes_transferred,
            completion_key,
            overlapped))
    {
        const DWORD error = ::GetLastError();

        throw std::system_error(
            static_cast<int>(error),
            std::system_category(),
            "PostQueuedCompletionStatus failed");
    }
}

HANDLE IocpContext::native_handle() const noexcept
{
    return handle_.get();
}

std::optional<IocpCompletion> IocpContext::wait(
    std::chrono::milliseconds timeout)
{
    DWORD timeout_ms{};

    if (timeout == std::chrono::milliseconds::max())
    {
        timeout_ms = INFINITE;
    }
    else
    {
        const auto count = timeout.count();

        timeout_ms = static_cast<DWORD>(
            std::clamp<std::int64_t>(
                count,
                0,
                static_cast<std::int64_t>(INFINITE) - 1));
    }

    DWORD bytes_transferred{};
    ULONG_PTR completion_key{};
    OVERLAPPED* overlapped{};

    const BOOL success = ::GetQueuedCompletionStatus(
        handle_.get(),
        &bytes_transferred,
        &completion_key,
        &overlapped,
        timeout_ms);

    const DWORD error =
        success ? ERROR_SUCCESS : ::GetLastError();

    if (overlapped == nullptr)
    {
        if (!success && error == WAIT_TIMEOUT)
        {
            return std::nullopt;
        }

        if (!success)
        {
            throw std::system_error(
                static_cast<int>(error),
                std::system_category(),
                "GetQueuedCompletionStatus");
        }

        return IocpCompletion{
            .bytes_transferred = bytes_transferred,
            .completion_key = completion_key,
            .operation = nullptr,
            .error = ERROR_SUCCESS
        };
    }

    IocpOperation* operation =
        CONTAINING_RECORD(
            overlapped,
            IocpOperation,
            overlapped_);

    return IocpCompletion{
        .bytes_transferred = bytes_transferred,
        .completion_key = completion_key,
        .operation = operation,
        .error = error
    };
}

} // namespace cwm::iocp
