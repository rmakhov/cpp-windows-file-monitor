#pragma once

#include <chrono>
#include <filesystem>

namespace cwm::filesystem
{

enum class FileSystemEventAction
{
    Added,
    Removed,
    Modified,
    RenamedOldName,
    RenamedNewName
};

struct FileSystemEvent
{
    std::filesystem::path path;
    FileSystemEventAction action;
    std::chrono::steady_clock::time_point timestamp;
};

} // namespace cwm::filesystem
