#include <gtest/gtest.h>

#include <cwm/iocp/iocp_operation.h>

TEST(IocpOperationTest, CreatesZeroInitializedOverlapped)
{
    cwm::iocp::IocpOperation operation;

    const OVERLAPPED* overlapped = operation.overlapped();

    ASSERT_NE(overlapped, nullptr);

    EXPECT_EQ(overlapped->Internal, 0u);
    EXPECT_EQ(overlapped->InternalHigh, 0u);
    EXPECT_EQ(overlapped->Offset, 0u);
    EXPECT_EQ(overlapped->OffsetHigh, 0u);
    EXPECT_EQ(overlapped->hEvent, nullptr);
}

TEST(IocpOperationTest, OverlappedAddressIsStable)
{
    cwm::iocp::IocpOperation operation;

    OVERLAPPED* first = operation.overlapped();
    OVERLAPPED* second = operation.overlapped();

    EXPECT_EQ(first, second);
}
