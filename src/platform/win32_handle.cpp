#include <cwm/platform/win32_handle.h>

namespace cwm::platform
{

Win32Handle::Win32Handle(HANDLE handle) noexcept
    : handle_(handle)
{
}

Win32Handle::~Win32Handle() noexcept
{
    reset();
}

Win32Handle::Win32Handle(Win32Handle&& other) noexcept
    : handle_(other.release())
{
}

Win32Handle& Win32Handle::operator=(Win32Handle&& other) noexcept
{
    if (this != &other)
    {
        reset(other.release());
    }

    return *this;
}

bool Win32Handle::valid() const noexcept
{
    return handle_ != nullptr;
}

HANDLE Win32Handle::get() const noexcept
{
    return handle_;
}

HANDLE Win32Handle::release() noexcept
{
    HANDLE handle = handle_;
    handle_ = nullptr;

    return handle;
}

void Win32Handle::reset(HANDLE handle) noexcept
{
    if (handle_ != nullptr)
    {
        ::CloseHandle(handle_);
    }

    handle_ = handle;
}

} // namespace cwm::platform
