#include <cwm/iocp/iocp_operation.h>

namespace cwm::iocp
{

OVERLAPPED* IocpOperation::overlapped() noexcept
{
    return &overlapped_;
}

const OVERLAPPED* IocpOperation::overlapped() const noexcept
{
    return &overlapped_;
}

} // namespace cwm::iocp
