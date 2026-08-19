#include <gtest/gtest.h>
#include <cwm/platform/win32_handle.h>

TEST(Win32HandleTest, DefaultConstructionCreatesInvalidHandle)
{
    cwm::platform::Win32Handle handle;

    EXPECT_FALSE(handle.valid());
    EXPECT_EQ(handle.get(), nullptr);
}

TEST(Win32HandleTest, TakesOwnershipOfValidHandle)
{
    HANDLE raw = ::CreateEventW(nullptr, FALSE, FALSE, nullptr);
    ASSERT_NE(raw, nullptr);

    cwm::platform::Win32Handle handle{raw};

    EXPECT_TRUE(handle.valid());
    EXPECT_EQ(handle.get(), raw);
}

TEST(Win32HandleTest, MoveConstructionTransfersOwnership)
{
    HANDLE raw = ::CreateEventW(nullptr, FALSE, FALSE, nullptr);
    ASSERT_NE(raw, nullptr);

    cwm::platform::Win32Handle source{raw};

    cwm::platform::Win32Handle destination{std::move(source)};

    EXPECT_FALSE(source.valid());
    EXPECT_EQ(source.get(), nullptr);

    EXPECT_TRUE(destination.valid());
    EXPECT_EQ(destination.get(), raw);
}

TEST(Win32HandleTest, MoveAssignment)
{
    HANDLE raw_source = ::CreateEventW(nullptr, FALSE, FALSE, nullptr);
    ASSERT_NE(raw_source, nullptr);

    HANDLE raw_destination = ::CreateEventW(nullptr, FALSE, FALSE, nullptr);
    ASSERT_NE(raw_destination, nullptr);

    cwm::platform::Win32Handle source{raw_source};
    cwm::platform::Win32Handle destination{raw_destination};

    destination = std::move(source);

    // Source no longer owns the handle.
    EXPECT_FALSE(source.valid());
    EXPECT_EQ(source.get(), nullptr);

    // Destination now owns the source handle.
    EXPECT_TRUE(destination.valid());
    EXPECT_EQ(destination.get(), raw_source);

    // The previous destination handle was closed.
    EXPECT_EQ(::WaitForSingleObject(raw_destination, 0), WAIT_FAILED);
    EXPECT_EQ(::GetLastError(), ERROR_INVALID_HANDLE);
}

TEST(Win32HandleTest, DestructorClosesHandle)
{
    HANDLE raw = ::CreateEventW(nullptr, FALSE, FALSE, nullptr);
    ASSERT_NE(raw, nullptr);

    {
        cwm::platform::Win32Handle handle{raw};

        EXPECT_TRUE(handle.valid());
        EXPECT_EQ(handle.get(), raw);
    }

    EXPECT_EQ(::WaitForSingleObject(raw, 0), WAIT_FAILED);
    EXPECT_EQ(::GetLastError(), ERROR_INVALID_HANDLE);
}