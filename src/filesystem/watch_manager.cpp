#include <cwm/filesystem/watch_manager.h>

#include <stdexcept>
#include <unordered_set>
#include <utility>
#include <vector>

namespace cwm::filesystem
{

WatchManager::WatchManager(
    iocp::IocpContext& iocp,
    concurrency::BoundedQueue<FileSystemEvent>& event_queue,
    WatchManagerConfig config)
    : iocp_(iocp)
    , event_queue_(event_queue)
    , config_(std::move(config))
{
}

WatchManager::~WatchManager()
{
    stop();
}

/*
void WatchManager::add_directory(
    const std::filesystem::path& directory)
{
    if (directory.empty())
    {
        throw std::invalid_argument(
            "WatchManager directory must not be empty");
    }

    if (watchers_.contains(directory))
    {
        throw std::logic_error(
            "Directory is already being watched");
    }

    const ULONG_PTR completion_key =
        next_completion_key_++;

    if (completion_key == 0)
    {
        throw std::overflow_error(
            "WatchManager completion key exhausted");
    }

    DirectoryWatcherConfig config;
    config.directory = directory;
    config.completion_key = completion_key;

    auto watcher = std::make_unique<DirectoryWatcher>(
        iocp_,
        event_queue_,
        std::move(config));

    watcher->start();

    watchers_.emplace(
        directory,
        std::move(watcher));
}
*/

void WatchManager::add_directory(
    const std::filesystem::path& directory)
{
    if (directory.empty())
    {
        throw std::invalid_argument(
            "WatchManager directory must not be empty");
    }

    if (watchers_.contains(directory))
    {
        throw std::logic_error(
            "Directory is already being watched");
    }

    add_watcher(directory);

    const auto subdirectories =
        discovery_.enumerate(directory);

    for (const auto& subdirectory : subdirectories)
    {
        if (!watchers_.contains(subdirectory))
        {
            add_watcher(subdirectory);
        }
    }
}

void WatchManager::remove_directory(
    const std::filesystem::path& directory)
{
    const auto iterator =
        watchers_.find(directory);

    if (iterator == watchers_.end())
    {
        return;
    }

    iterator->second->stop();
}

void WatchManager::add_watcher(
    const std::filesystem::path& directory)
{
    const ULONG_PTR completion_key =
        next_completion_key_++;

    if (completion_key == 0)
    {
        throw std::overflow_error(
            "WatchManager completion key exhausted");
    }

    DirectoryWatcherConfig config;
    config.directory = directory;
    config.completion_key = completion_key;
	
	config.notification_buffer_size =
		config_.notification_buffer_size;

	config.deferred_event_capacity =
		config_.deferred_event_capacity;
	
	config.event_accepted_callback =
    [this](const FileSystemEvent& event)
    {
        if (event.action != FileSystemEventAction::Added)
        {
            return;
        }

        std::error_code error;

        if (!std::filesystem::is_directory(
                event.path,
                error))
        {
            return;
        }

        if (error)
        {
            return;
        }

        if (watchers_.contains(event.path))
        {
            return;
        }

        add_watcher(event.path);
    };

    auto watcher = std::make_unique<DirectoryWatcher>(
        iocp_,
        event_queue_,
        std::move(config));

    watcher->start();

    watchers_.emplace(
        directory,
        std::move(watcher));
}

void WatchManager::reconcile_directory(
    const std::filesystem::path& directory)
{
    const auto discovered =
        discovery_.enumerate_recursive(directory);

    std::unordered_set<std::filesystem::path>
        expected;

    expected.reserve(discovered.size() + 1);

    // The root itself must always have a watcher.
    expected.insert(directory);

    for (const auto& path : discovered)
    {
        expected.insert(path);
    }

    std::vector<std::filesystem::path>
        to_remove;

    // Find stale watchers belonging to this root.
    for (const auto& [path, watcher] : watchers_)
    {
        if (!is_within_directory(path, directory))
        {
            continue;
        }

        if (!expected.contains(path))
        {
            to_remove.push_back(path);
        }
    }

    // Stop and remove stale watchers.
	for (const auto& path : to_remove)
	{
		const auto iterator =
			watchers_.find(path);

		if (iterator == watchers_.end())
		{
			continue;
		}

		iterator->second->stop();
	}

    // Add missing watchers.
    for (const auto& path : expected)
    {
        if (!watchers_.contains(path))
        {
            add_watcher(path);
        }
    }
}

/*
bool WatchManager::handle_completion(
    const iocp::IocpCompletion& completion)
{
    for (auto iterator = watchers_.begin();
         iterator != watchers_.end();
         ++iterator)
    {
        DirectoryWatcher& watcher =
            *iterator->second;

        if (watcher.handle_completion(completion))
        {
            if (watcher.state() ==
                DirectoryWatcherState::Stopped)
            {
                watchers_.erase(iterator);
            }

            return true;
        }
    }

    return false;
}
*/

bool WatchManager::handle_completion(
    const iocp::IocpCompletion& completion)
{
    for (auto iterator = watchers_.begin();
         iterator != watchers_.end();
         ++iterator)
    {
        DirectoryWatcher& watcher =
            *iterator->second;

        if (!watcher.handle_completion(completion))
        {
            continue;
        }

        const auto directory =
            iterator->first;

        if (watcher.state() ==
            DirectoryWatcherState::ReconciliationRequired)
        {
            pending_reconciliation_.insert(directory);

            watcher.stop();

            return true;
        }

        if (watcher.state() ==
            DirectoryWatcherState::Stopped)
        {
            const bool reconcile =
                pending_reconciliation_.erase(directory) > 0;

            watchers_.erase(iterator);

            if (reconcile)
            {
                reconcile_directory(directory);
            }

            return true;
        }

        return true;
    }

    return false;
}

void WatchManager::stop()
{
    for (auto& [directory, watcher] : watchers_)
    {
        watcher->stop();
    }
}

std::size_t WatchManager::watcher_count() const noexcept
{
    return watchers_.size();
}

bool WatchManager::is_within_directory(
    const std::filesystem::path& path,
    const std::filesystem::path& root) const
{
    auto path_iterator = path.begin();
    auto root_iterator = root.begin();

    for (; root_iterator != root.end();
         ++root_iterator, ++path_iterator)
    {
        if (path_iterator == path.end() ||
            *path_iterator != *root_iterator)
        {
            return false;
        }
    }

    return true;
}

} // namespace cwm::filesystem
