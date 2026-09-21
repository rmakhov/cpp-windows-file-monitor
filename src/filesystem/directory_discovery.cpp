#include <cwm/filesystem/directory_discovery.h>

#include <system_error>

namespace cwm::filesystem
{

std::vector<std::filesystem::path>
DirectoryDiscovery::enumerate(
    const std::filesystem::path& directory) const
{
    std::vector<std::filesystem::path> directories;

    std::error_code error;

    std::filesystem::directory_iterator iterator(
        directory,
        std::filesystem::directory_options::skip_permission_denied,
        error);

    if (error)
    {
        throw std::filesystem::filesystem_error(
            "Failed to enumerate directory",
            directory,
            error);
    }

    const auto end =
        std::filesystem::directory_iterator{};

    for (; iterator != end; iterator.increment(error))
    {
        if (error)
        {
            throw std::filesystem::filesystem_error(
                "Failed while enumerating directory",
                directory,
                error);
        }

        std::error_code status_error;

        if (iterator->is_directory(status_error))
        {
            if (status_error)
            {
                throw std::filesystem::filesystem_error(
                    "Failed to inspect directory entry",
                    iterator->path(),
                    status_error);
            }

            directories.push_back(iterator->path());
        }
    }

    return directories;
}

} // namespace cwm::filesystem

