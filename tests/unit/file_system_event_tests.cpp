#include <cwm/filesystem/file_system_event.h>

#include <gtest/gtest.h>

#include <filesystem>

namespace
{

TEST(FileSystemEventTest, StoresPathAndAction)
{
    const std::filesystem::path path =
        LR"(C:\Data\example.txt)";

    const cwm::filesystem::FileSystemEvent event{
        path,
        cwm::filesystem::FileSystemEventAction::Added
    };

    EXPECT_EQ(event.path, path);
    EXPECT_EQ(
        event.action,
        cwm::filesystem::FileSystemEventAction::Added);
}

TEST(FileSystemEventTest, SupportsAllActions)
{
    using cwm::filesystem::FileSystemEventAction;

    EXPECT_NE(
        FileSystemEventAction::Added,
        FileSystemEventAction::Removed);

    EXPECT_NE(
        FileSystemEventAction::Modified,
        FileSystemEventAction::RenamedOldName);

    EXPECT_NE(
        FileSystemEventAction::RenamedOldName,
        FileSystemEventAction::RenamedNewName);
}

} // namespace
