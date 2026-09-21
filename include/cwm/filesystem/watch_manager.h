#pragma once

#include <cwm/concurrency/bounded_queue.h>
#include <cwm/filesystem/directory_discovery.h>
#include <cwm/filesystem/directory_watcher.h>
#include <cwm/filesystem/file_system_event.h>
#include <cwm/iocp/iocp_completion.h>
#include <cwm/iocp/iocp_context.h>

#include <cstddef>
#include <filesystem>
#include <memory>
#include <unordered_map>
#include <unordered_set>

namespace cwm::filesystem
{
	
struct WatchManagerConfig
{
    std::size_t notification_buffer_size = 64 * 1024;
    std::size_t deferred_event_capacity = 512;
};

class WatchManager final
{
public:
    WatchManager(
        iocp::IocpContext& iocp,
        concurrency::BoundedQueue<FileSystemEvent>& event_queue,
		 WatchManagerConfig config = {});

    ~WatchManager();

    WatchManager(const WatchManager&) = delete;
    WatchManager& operator=(const WatchManager&) = delete;

    WatchManager(WatchManager&&) = delete;
    WatchManager& operator=(WatchManager&&) = delete;

    void add_directory(
        const std::filesystem::path& directory);

    void remove_directory(
        const std::filesystem::path& directory);
		
	void add_watcher(
		const std::filesystem::path& directory);

    [[nodiscard]]
    bool handle_completion(
        const iocp::IocpCompletion& completion);

    void stop();

    [[nodiscard]]
    std::size_t watcher_count() const noexcept;

private:
    using WatcherPtr =
        std::unique_ptr<DirectoryWatcher>;

    using WatcherMap =
        std::unordered_map<
            std::filesystem::path,
            WatcherPtr>;

    iocp::IocpContext& iocp_;
    concurrency::BoundedQueue<FileSystemEvent>& event_queue_;
	
	WatchManagerConfig config_;

    WatcherMap watchers_;
    DirectoryDiscovery discovery_;

    ULONG_PTR next_completion_key_{1};

    std::unordered_set<std::filesystem::path>
        pending_reconciliation_;

    void reconcile_directory(
        const std::filesystem::path& directory);

    [[nodiscard]]
    bool is_within_directory(
        const std::filesystem::path& path,
        const std::filesystem::path& root) const;
};

} // namespace cwm::filesystem
