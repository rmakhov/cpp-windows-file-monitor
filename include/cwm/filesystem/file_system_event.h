#pragma once

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
};

} // namespace cwm::filesystem
