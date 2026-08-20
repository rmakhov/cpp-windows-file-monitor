#include <gtest/gtest.h>

#include <cwm/iocp/iocp_context.h>

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

