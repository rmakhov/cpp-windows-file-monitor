#pragma once

#include <filesystem>
#include <vector>

namespace cwm::filesystem
{

class DirectoryDiscovery final
{
public:
    DirectoryDiscovery() = default;

    DirectoryDiscovery(const DirectoryDiscovery&) = delete;
    DirectoryDiscovery& operator=(const DirectoryDiscovery&) = delete;

    DirectoryDiscovery(DirectoryDiscovery&&) = default;
    DirectoryDiscovery& operator=(DirectoryDiscovery&&) = default;

    ~DirectoryDiscovery() = default;

    [[nodiscard]]
    std::vector<std::filesystem::path> enumerate(
        const std::filesystem::path& directory) const;
		
	[[nodiscard]]
	std::vector<std::filesystem::path> enumerate_recursive(
		const std::filesystem::path& directory) const;
};

} // namespace cwm::filesystem
