#pragma once

#include <Windows.h>

namespace cwm::iocp
{

class IocpContext;

class IocpOperation final
{
    friend class IocpContext;

public:
    IocpOperation() noexcept = default;

    ~IocpOperation() noexcept = default;

    IocpOperation(const IocpOperation&) = delete;
    IocpOperation& operator=(const IocpOperation&) = delete;

    IocpOperation(IocpOperation&&) = delete;
    IocpOperation& operator=(IocpOperation&&) = delete;

    [[nodiscard]]
    OVERLAPPED* overlapped() noexcept;

    [[nodiscard]]
    const OVERLAPPED* overlapped() const noexcept;

private:
    OVERLAPPED overlapped_{};
};

} // namespace cwm::iocp
