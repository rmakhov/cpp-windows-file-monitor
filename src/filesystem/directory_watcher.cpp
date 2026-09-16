#include <cwm/filesystem/directory_watcher.h>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace cwm::filesystem
{

namespace
{

constexpr DWORD kNotifyFilter =
    FILE_NOTIFY_CHANGE_FILE_NAME |
    FILE_NOTIFY_CHANGE_DIR_NAME |
    FILE_NOTIFY_CHANGE_ATTRIBUTES |
    FILE_NOTIFY_CHANGE_SIZE |
    FILE_NOTIFY_CHANGE_LAST_WRITE |
    FILE_NOTIFY_CHANGE_CREATION;

FileSystemEventAction
to_event_action(DWORD action)
{
    switch (action)
    {
    case FILE_ACTION_ADDED:
        return FileSystemEventAction::Added;

    case FILE_ACTION_REMOVED:
        return FileSystemEventAction::Removed;

    case FILE_ACTION_MODIFIED:
        return FileSystemEventAction::Modified;

    case FILE_ACTION_RENAMED_OLD_NAME:
        return FileSystemEventAction::RenamedOldName;

    case FILE_ACTION_RENAMED_NEW_NAME:
        return FileSystemEventAction::RenamedNewName;

    default:
        throw std::runtime_error(
            "Unknown FILE_NOTIFY_INFORMATION action");
    }
}

} // namespace

DirectoryWatcher::DirectoryWatcher(
    iocp::IocpContext& iocp,
    concurrency::BoundedQueue<FileSystemEvent>& event_queue,
    DirectoryWatcherConfig config)
    : iocp_(iocp)
    , event_queue_(event_queue)
    , config_(std::move(config))
    , notification_buffer_(config_.notification_buffer_size)
{
    if (config_.directory.empty())
    {
        throw std::invalid_argument(
            "DirectoryWatcher directory must not be empty");
    }

    if (config_.completion_key == 0)
    {
        throw std::invalid_argument(
            "DirectoryWatcher completion key must not be zero");
    }

    if (config_.notification_buffer_size == 0)
    {
        throw std::invalid_argument(
            "DirectoryWatcher notification buffer size must be greater than zero");
    }

    if (config_.deferred_event_capacity == 0)
    {
        throw std::invalid_argument(
            "DirectoryWatcher deferred event capacity must be greater than zero");
    }

    //deferred_events_.resize(0);
}

DirectoryWatcher::~DirectoryWatcher() noexcept
{
    if (state_ != DirectoryWatcherState::Stopped)
    {
        std::terminate();
    }
}

void DirectoryWatcher::start()
{
    if (state_ != DirectoryWatcherState::Stopped)
    {
        throw std::logic_error(
            "DirectoryWatcher is already running");
    }

    open_directory();

    iocp_.associate(
        directory_handle_.get(),
        config_.completion_key);

    state_ = DirectoryWatcherState::Running;

    try
    {
        arm_read();
    }
    catch (...)
    {
        state_ = DirectoryWatcherState::Stopped;
        directory_handle_.reset();
        throw;
    }
}

void DirectoryWatcher::stop()
{
    if (state_ == DirectoryWatcherState::Stopped)
    {
        return;
    }

    if (state_ == DirectoryWatcherState::Stopping)
    {
        return;
    }

    state_ = DirectoryWatcherState::Stopping;

    if (directory_handle_.valid())
    {
        if (!CancelIoEx(
                directory_handle_.get(),
                operation_.overlapped()))
        {
            const DWORD error = GetLastError();

            if (error != ERROR_NOT_FOUND)
            {
                // There may already be a completion queued or the
                // operation may have completed synchronously.
                //
                // We deliberately don't throw from stop()/destructor.
            }
        }
    }
}

bool DirectoryWatcher::handle_completion(
    const iocp::IocpCompletion& completion)
{
    if (completion.completion_key != config_.completion_key)
    {
        return false;
    }

    if (completion.operation != &operation_)
    {
        return false;
    }

    if (state_ == DirectoryWatcherState::Stopped)
    {
        return true;
    }
	/*
    if (state_ == DirectoryWatcherState::Stopping)
    {
        if (completion.error == ERROR_OPERATION_ABORTED)
        {
            directory_handle_.reset();
            state_ = DirectoryWatcherState::Stopped;
        }

        return true;
    }
	*/
	
	if (state_ == DirectoryWatcherState::Stopping)
	{
		directory_handle_.reset();
		state_ = DirectoryWatcherState::Stopped;
		return true;
	}

    if (completion.error != ERROR_SUCCESS)
    {
        handle_read_error(completion.error);

        if (state_ != DirectoryWatcherState::Stopping &&
            state_ != DirectoryWatcherState::Stopped)
        {
            arm_read();
        }

        return true;
    }

    if (completion.bytes_transferred == 0)
    {
        mark_reconciliation_required(
            ReconciliationReason::WindowsNotificationOverflow);

        if (state_ == DirectoryWatcherState::ReconciliationRequired)
        {
            arm_read();
        }

        return true;
    }

    process_notifications(
        completion.bytes_transferred);

    if (state_ == DirectoryWatcherState::Running ||
        state_ == DirectoryWatcherState::ReconciliationRequired)
    {
        arm_read();
    }

    return true;
}

std::size_t DirectoryWatcher::retry_deferred_events()
{
    std::size_t accepted = 0;

    while (!deferred_events_.empty())
    {
        FileSystemEvent& event = deferred_events_.front();

        const auto result = event_queue_.try_push(event);

        if (result != concurrency::QueuePushResult::Accepted)
        {
            break;
        }

        deferred_events_.pop_front();
        ++accepted;
    }

    return accepted;
}

DirectoryWatcherState DirectoryWatcher::state() const noexcept
{
    return state_;
}

ReconciliationReason
DirectoryWatcher::reconciliation_reason() const noexcept
{
    return reconciliation_reason_;
}

const std::filesystem::path&
DirectoryWatcher::directory() const noexcept
{
    return config_.directory;
}

std::size_t
DirectoryWatcher::deferred_event_count() const noexcept
{
    return deferred_events_.size();
}

void DirectoryWatcher::open_directory()
{
    HANDLE handle = CreateFileW(
        config_.directory.c_str(),
        FILE_LIST_DIRECTORY,
        FILE_SHARE_READ |
            FILE_SHARE_WRITE |
            FILE_SHARE_DELETE,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS |
            FILE_FLAG_OVERLAPPED,
        nullptr);

    if (handle == INVALID_HANDLE_VALUE)
    {
        throw std::system_error(
            static_cast<int>(GetLastError()),
            std::system_category(),
            "CreateFileW failed for directory");
    }

    directory_handle_.reset(handle);
}

void DirectoryWatcher::arm_read()
{
    std::memset(
        operation_.overlapped(),
        0,
        sizeof(OVERLAPPED));

    BOOL result = ReadDirectoryChangesW(
        directory_handle_.get(),
        notification_buffer_.data(),
        static_cast<DWORD>(notification_buffer_.size()),
        FALSE,
        kNotifyFilter,
        nullptr,
        operation_.overlapped(),
        nullptr);

    if (result != FALSE)
    {
        return;
    }

    const DWORD error = GetLastError();

    if (error == ERROR_IO_PENDING)
    {
        return;
    }

    if (error == ERROR_NOTIFY_ENUM_DIR)
    {
        mark_reconciliation_required(
            ReconciliationReason::WindowsNotificationOverflow);

        return;
    }

    handle_read_error(error);
}

void DirectoryWatcher::process_notifications(
    DWORD bytes_transferred)
{
    if (bytes_transferred > notification_buffer_.size())
    {
        mark_reconciliation_required(
            ReconciliationReason::WatcherError);

        return;
    }

    std::size_t offset = 0;

    while (offset < bytes_transferred)
    {
        const auto* notification =
            reinterpret_cast<const FILE_NOTIFY_INFORMATION*>(
                notification_buffer_.data() + offset);

        const std::size_t remaining =
            bytes_transferred - offset;

        if (remaining < sizeof(FILE_NOTIFY_INFORMATION))
        {
            mark_reconciliation_required(
                ReconciliationReason::WatcherError);

            return;
        }

        //const std::size_t record_size =
        //    sizeof(FILE_NOTIFY_INFORMATION) +
        //    notification->FileNameLength;
			
		const std::size_t record_size =
			FIELD_OFFSET(FILE_NOTIFY_INFORMATION, FileName) +
			notification->FileNameLength;

        if (record_size > remaining)
        {
            mark_reconciliation_required(
                ReconciliationReason::WatcherError);

            return;
        }

        if (notification->FileNameLength %
                sizeof(WCHAR) != 0)
        {
            mark_reconciliation_required(
                ReconciliationReason::WatcherError);

            return;
        }

        const std::size_t character_count =
            notification->FileNameLength / sizeof(WCHAR);

        const std::wstring_view file_name(
            notification->FileName,
            character_count);

        FileSystemEvent event{
            config_.directory / file_name,
            to_event_action(notification->Action),
            std::chrono::steady_clock::now()};

        handle_event(std::move(event));

        if (notification->NextEntryOffset == 0)
        {
            break;
        }

        if (notification->NextEntryOffset < record_size ||
            notification->NextEntryOffset > remaining)
        {
            mark_reconciliation_required(
                ReconciliationReason::WatcherError);

            return;
        }

        offset += notification->NextEntryOffset;
    }
}

void DirectoryWatcher::handle_event(
    FileSystemEvent event)
{
    const auto result = event_queue_.try_push(event);

    if (result == concurrency::QueuePushResult::Accepted)
    {
        return;
    }

    if (result == concurrency::QueuePushResult::Closed)
    {
        mark_reconciliation_required(
            ReconciliationReason::EventQueueOverflow);

        return;
    }

    if (deferred_events_.size() >=
        config_.deferred_event_capacity)
    {
        mark_reconciliation_required(
            ReconciliationReason::DeferredEventOverflow);

        return;
    }

    deferred_events_.push_back(std::move(event));
}

void DirectoryWatcher::mark_reconciliation_required(
    ReconciliationReason reason) noexcept
{
    if (state_ == DirectoryWatcherState::Stopping ||
        state_ == DirectoryWatcherState::Stopped)
    {
        return;
    }

    state_ = DirectoryWatcherState::ReconciliationRequired;

    if (reconciliation_reason_ == ReconciliationReason::None)
    {
        reconciliation_reason_ = reason;
    }
}

void DirectoryWatcher::handle_read_error(
    DWORD error)
{
    if (error == ERROR_OPERATION_ABORTED)
    {
        if (state_ == DirectoryWatcherState::Stopping)
        {
            directory_handle_.reset();
            state_ = DirectoryWatcherState::Stopped;
        }

        return;
    }

    mark_reconciliation_required(
        ReconciliationReason::WatcherError);
}

} // namespace cwm::filesystem
