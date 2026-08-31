#include <Windows.h>

#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

#include <gtest/gtest.h>

#include <cwm/iocp/iocp_context.h>
#include <cwm/iocp/iocp_operation.h>
#include <cwm/platform/win32_handle.h>

namespace
{

class IocpReadFileTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        file_path_ =
            std::filesystem::temp_directory_path() /
            L"cwm_iocp_test.bin";

        constexpr char test_data[] = "Hello IOCP!";

        std::ofstream file(
            file_path_,
            std::ios::binary | std::ios::trunc);

        ASSERT_TRUE(file.is_open());

        file.write(
            test_data,
            sizeof(test_data) - 1);

        ASSERT_TRUE(file.good());
    }

    void TearDown() override
    {
        std::error_code ec;

        std::filesystem::remove(
            file_path_,
            ec);
    }

    std::filesystem::path file_path_;
};

} // namespace

TEST_F(
    IocpReadFileTest,
    ReadFileProducesIocpCompletion)
{
    cwm::iocp::IocpContext context;

    HANDLE raw_file = ::CreateFileW(
        file_path_.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_OVERLAPPED,
        nullptr);

    ASSERT_NE(raw_file, INVALID_HANDLE_VALUE);

	cwm::platform::Win32Handle file{raw_file};

	context.associate(
		file.get(),
		123);

	cwm::iocp::IocpOperation operation;

	std::array<char, 64> buffer{};

	const BOOL read_result = ::ReadFile(
		file.get(),
		buffer.data(),
		static_cast<DWORD>(buffer.size()),
		nullptr,
		operation.overlapped());

	if (!read_result)
	{
		const DWORD error = ::GetLastError();

		ASSERT_EQ(error, ERROR_IO_PENDING)
			<< "ReadFile failed with error "
			<< error;
	}

    const auto completion =
        context.wait(std::chrono::seconds(2));

    ASSERT_TRUE(completion.has_value());

    EXPECT_EQ(completion->completion_key, 123u);
    EXPECT_EQ(
        completion->operation,
        &operation);

    ASSERT_EQ(
        completion->error,
        ERROR_SUCCESS);

    EXPECT_EQ(
        completion->bytes_transferred,
        11u);

    EXPECT_EQ(
        std::string(
            buffer.data(),
            completion->bytes_transferred),
        "Hello IOCP!");
}
