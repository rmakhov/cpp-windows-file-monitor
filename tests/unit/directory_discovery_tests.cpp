#include <cwm/filesystem/directory_discovery.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace
{

class DirectoryDiscoveryTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        test_root_ =
            std::filesystem::temp_directory_path() /
            ("cwm_directory_discovery_test_" +
             std::to_string(
                 std::chrono::steady_clock::now()
                     .time_since_epoch()
                     .count()));

        std::filesystem::create_directories(test_root_);
    }

    void TearDown() override
    {
        std::error_code error;
        std::filesystem::remove_all(test_root_, error);
    }

    std::filesystem::path create_directory(
        const std::string& name)
    {
        const auto directory =
            test_root_ / name;

        EXPECT_TRUE(
            std::filesystem::create_directories(directory));

        return directory;
    }

    void create_file(
        const std::filesystem::path& path)
    {
        std::ofstream file(path);

        ASSERT_TRUE(file.is_open());

        file << "test";
    }

    std::filesystem::path test_root_;
    cwm::filesystem::DirectoryDiscovery discovery_;
};

TEST_F(
    DirectoryDiscoveryTest,
    EnumeratesImmediateSubdirectories)
{
    const auto directory_a =
        create_directory("directory_a");

    const auto directory_b =
        create_directory("directory_b");

    const auto nested =
        std::filesystem::create_directories(
            directory_a / "nested");

    ASSERT_TRUE(nested);

    const auto directories =
        discovery_.enumerate(test_root_);

    EXPECT_EQ(directories.size(), 2u);

    EXPECT_NE(
        std::find(
            directories.begin(),
            directories.end(),
            directory_a),
        directories.end());

    EXPECT_NE(
        std::find(
            directories.begin(),
            directories.end(),
            directory_b),
        directories.end());

    EXPECT_EQ(
        std::find(
            directories.begin(),
            directories.end(),
            directory_a / "nested"),
        directories.end());
}

TEST_F(
    DirectoryDiscoveryTest,
    IgnoresFiles)
{
    create_directory("directory");

    const auto file =
        test_root_ / "file.txt";

    create_file(file);

    const auto directories =
        discovery_.enumerate(test_root_);

    ASSERT_EQ(directories.size(), 1u);

    EXPECT_EQ(
        directories.front(),
        test_root_ / "directory");
}

TEST_F(
    DirectoryDiscoveryTest,
    HandlesEmptyDirectory)
{
    const auto directories =
        discovery_.enumerate(test_root_);

    EXPECT_TRUE(directories.empty());
}

TEST_F(
    DirectoryDiscoveryTest,
    HandlesMissingDirectory)
{
    const auto missing =
        test_root_ / "does_not_exist";

    bool exception_thrown = false;

    try
    {
        const auto directories =
            discovery_.enumerate(missing);

        (void)directories;
    }
    catch (const std::filesystem::filesystem_error&)
    {
        exception_thrown = true;
    }

    EXPECT_TRUE(exception_thrown);
}

TEST_F(
    DirectoryDiscoveryTest,
    EnumerateRecursiveReturnsEmptyForEmptyDirectory)
{
    const auto result =
        discovery_.enumerate_recursive(test_root_);

    EXPECT_TRUE(result.empty());
}

TEST_F(
    DirectoryDiscoveryTest,
    EnumerateRecursiveFindsNestedDirectories)
{
    const auto level1 =
        create_directory("level1");

    const auto level2 =
        std::filesystem::create_directories(
            level1 / "level2");

    ASSERT_TRUE(level2);

    const auto level3 =
        std::filesystem::create_directories(
            level1 / "level2" / "level3");

    ASSERT_TRUE(level3);

    const auto result =
        discovery_.enumerate_recursive(test_root_);

    EXPECT_EQ(result.size(), 3u);

    EXPECT_NE(
        std::find(
            result.begin(),
            result.end(),
            level1),
        result.end());

    EXPECT_NE(
        std::find(
            result.begin(),
            result.end(),
            level1 / "level2"),
        result.end());

    EXPECT_NE(
        std::find(
            result.begin(),
            result.end(),
            level1 / "level2" / "level3"),
        result.end());
}

TEST_F(
    DirectoryDiscoveryTest,
    EnumerateRecursiveIgnoresFiles)
{
    const auto directory =
        create_directory("directory");

    create_file(
        test_root_ / "file.txt");

    create_file(
        directory / "nested_file.txt");

    const auto result =
        discovery_.enumerate_recursive(test_root_);

    ASSERT_EQ(result.size(), 1u);

    EXPECT_EQ(
        result.front(),
        directory);
}

} // namespace

