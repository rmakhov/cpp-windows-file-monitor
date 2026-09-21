#pragma once

#include <cwm/concurrency/bounded_queue.h>
#include <cwm/filesystem/file_system_event.h>
#include <cwm/iocp/iocp_completion.h>
#include <cwm/iocp/iocp_context.h>
#include <cwm/iocp/iocp_operation.h>
#include <cwm/platform/win32_handle.h>

#include <cstddef>
#include <deque>
#include <filesystem>
#include <functional>
#include <vector>

#include <Windows.h>

namespace cwm::filesystem
{
	
using EventAcceptedCallback = std::function<void(const FileSystemEvent&)>;

struct DirectoryWatcherConfig
{
    std::filesystem::path directory;

    // Identifies this watcher when completions are retrieved
    // from the shared IOCP.
    ULONG_PTR completion_key{};
	
	EventAcceptedCallback event_accepted_callback;

    // Buffer used by ReadDirectoryChangesW.
    std::size_t notification_buffer_size = 64 * 1024;

    // Maximum number of events retained locally when the
    // external event queue is temporarily full.
    std::size_t deferred_event_capacity = 512;
};

enum class DirectoryWatcherState
{
    Stopped,
    Running,
    ReconciliationRequired,
    Stopping
};

enum class ReconciliationReason
{
    None,
    EventQueueOverflow,
    DeferredEventOverflow,
    WindowsNotificationOverflow,
    WatcherError
};

class DirectoryWatcher
{
public:
    DirectoryWatcher(
        iocp::IocpContext& iocp,
        concurrency::BoundedQueue<FileSystemEvent>& event_queue,
        DirectoryWatcherConfig config);

    ~DirectoryWatcher();

    DirectoryWatcher(const DirectoryWatcher&) = delete;
    DirectoryWatcher& operator=(const DirectoryWatcher&) = delete;

    DirectoryWatcher(DirectoryWatcher&&) = delete;
    DirectoryWatcher& operator=(DirectoryWatcher&&) = delete;

    void start();

    void stop();

    [[nodiscard]]
    bool handle_completion(
        const iocp::IocpCompletion& completion);

    [[nodiscard]]
    std::size_t retry_deferred_events();

    [[nodiscard]]
    DirectoryWatcherState state() const noexcept;

    [[nodiscard]]
    ReconciliationReason reconciliation_reason() const noexcept;

    [[nodiscard]]
    const std::filesystem::path& directory() const noexcept;

    [[nodiscard]]
    std::size_t deferred_event_count() const noexcept;

private:
    void open_directory();

    void arm_read();

    void process_notifications(
        DWORD bytes_transferred);

    void handle_event(
        FileSystemEvent event);

    void mark_reconciliation_required(
        ReconciliationReason reason) noexcept;

    void handle_read_error(
        DWORD error);
		
	void notify_event_accepted(const FileSystemEvent& event);

    iocp::IocpContext& iocp_;

    concurrency::BoundedQueue<FileSystemEvent>& event_queue_;

    DirectoryWatcherConfig config_;

    platform::Win32Handle directory_handle_;

    // One outstanding ReadDirectoryChangesW operation.
    // The embedded OVERLAPPED must remain alive until its
    // completion has been consumed.
    iocp::IocpOperation operation_;

    std::vector<std::byte> notification_buffer_;

    // Events temporarily retained when the external queue
    // is full.
    std::deque<FileSystemEvent> deferred_events_;

    DirectoryWatcherState state_{
        DirectoryWatcherState::Stopped};

    ReconciliationReason reconciliation_reason_{
        ReconciliationReason::None};
};

} // namespace cwm::filesystem
