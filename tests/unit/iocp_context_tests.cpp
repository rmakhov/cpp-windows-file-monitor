#include <gtest/gtest.h>

#include <cwm/iocp/iocp_context.h>
#include <cwm/iocp/iocp_operation.h>

TEST(IocpContextTest, CreatesValidIocp)
{
    cwm::iocp::IocpContext iocp;

    EXPECT_NE(iocp.native_handle(), nullptr);
}

TEST(IocpContextTest, MoveConstruction)
{
    cwm::iocp::IocpContext source;

    HANDLE raw = source.native_handle();

    cwm::iocp::IocpContext destination{
        std::move(source)
    };

    EXPECT_EQ(destination.native_handle(), raw);
    EXPECT_EQ(source.native_handle(), nullptr);
}

TEST(IocpContextTest, MoveAssignment)
{
    cwm::iocp::IocpContext source;
    HANDLE source_handle = source.native_handle();

    cwm::iocp::IocpContext destination;
    HANDLE destination_handle = destination.native_handle();

    ASSERT_NE(source_handle, nullptr);
    ASSERT_NE(destination_handle, nullptr);

    destination = std::move(source);

    EXPECT_EQ(source.native_handle(), nullptr);
    EXPECT_EQ(destination.native_handle(), source_handle);
}

TEST(IocpContextTest, WaitReturnsPostedCompletion)
{
    cwm::iocp::IocpContext context;

    cwm::iocp::IocpOperation operation;

    context.post(
        456,
        123,
        operation.overlapped());

    const auto completion =
        context.wait(std::chrono::seconds(1));

    ASSERT_TRUE(completion.has_value());

    EXPECT_EQ(completion->bytes_transferred, 456u);
    EXPECT_EQ(completion->completion_key, 123u);
    EXPECT_EQ(completion->operation, &operation);
    EXPECT_EQ(completion->error, ERROR_SUCCESS);
}

TEST(IocpContextTest, WaitReturnsNulloptOnTimeout)
{
    cwm::iocp::IocpContext context;

    const auto completion =
        context.wait(std::chrono::milliseconds(10));

    EXPECT_FALSE(completion.has_value());
}

TEST(IocpContextTest, WaitReturnsCompletionWithoutOperation)
{
    cwm::iocp::IocpContext context;

    context.post(
        321,
        789,
        nullptr);

    const auto completion =
        context.wait(std::chrono::seconds(1));

    ASSERT_TRUE(completion.has_value());

    EXPECT_EQ(completion->bytes_transferred, 321u);
    EXPECT_EQ(completion->completion_key, 789u);
    EXPECT_EQ(completion->operation, nullptr);
    EXPECT_EQ(completion->error, ERROR_SUCCESS);
}



