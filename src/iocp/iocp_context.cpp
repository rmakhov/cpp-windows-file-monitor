#include <cwm/iocp/iocp_context.h>

#include <stdexcept>
#include <system_error>

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

} // namespace cwm::iocp
