#pragma once

#include <Windows.h>

namespace cwm::iocp
{

class IocpOperation;

struct IocpCompletion
{
    DWORD bytes_transferred{};
    ULONG_PTR completion_key{};
    IocpOperation* operation{};
    DWORD error{};
};

} // namespace cwm::iocp
