#include <cwm/filesystem/watch_manager.h>

#include <stdexcept>
#include <utility>

namespace cwm::filesystem
{

WatchManager::WatchManager(
    iocp::IocpContext& iocp,
    concurrency::BoundedQueue<FileSystemEvent>& event_queue)
    : iocp_(iocp)
    , event_queue_(event_queue)
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

    auto watcher = std::make_unique<DirectoryWatcher>(
        iocp_,
        event_queue_,
        std::move(config));

    watcher->start();

    watchers_.emplace(
        directory,
        std::move(watcher));
}

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

} // namespace cwm::filesystem
