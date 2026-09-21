#include <cwm/concurrency/bounded_queue.h>
#include <cwm/filesystem/file_system_event.h>
#include <cwm/filesystem/watch_manager.h>
#include <cwm/iocp/iocp_context.h>

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <stop_token>
#include <string>
#include <thread>
#include <vector>

namespace
{

class WatchManagerTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        test_root_ =
            std::filesystem::temp_directory_path() /
            ("cwm_watch_manager_test_" +
             std::to_string(
                 std::chrono::steady_clock::now()
                     .time_since_epoch()
                     .count()));

        std::filesystem::create_directories(test_root_);
    }

    void TearDown() override
    {
        std::error_code error;
        std::filesystem::remove_all(test_root_, error);
    }

    static std::filesystem::path create_directory(
        const std::filesystem::path& parent,
        const std::string& name)
    {
        const auto directory = parent / name;

        std::filesystem::create_directories(directory);

        return directory;
    }

    static void create_file(const std::filesystem::path& path)
    {
        std::ofstream file(path);
        ASSERT_TRUE(file.is_open());

        file << "test";
        file.close();
    }

    std::filesystem::path test_root_;
};

/*
TEST_F(WatchManagerTest, AddDirectoryStartsWatcher)
{
    cwm::iocp::IocpContext iocp;

    cwm::concurrency::BoundedQueue<cwm::filesystem::FileSystemEvent>
        event_queue(16);

    cwm::filesystem::WatchManager manager(iocp, event_queue);

    const auto directory =
        create_directory(test_root_, "directory");

    manager.add_directory(directory);

    EXPECT_EQ(manager.watcher_count(), 1u);
}
*/

TEST_F(WatchManagerTest, AddDirectoryStartsWatcher)
{
    cwm::iocp::IocpContext iocp;

    cwm::concurrency::BoundedQueue<cwm::filesystem::FileSystemEvent>
        event_queue(16);

    cwm::filesystem::WatchManager manager(iocp, event_queue);

    const auto directory =
        create_directory(test_root_, "directory");

    manager.add_directory(directory);

    ASSERT_EQ(manager.watcher_count(), 1u);

    manager.stop();

    const auto deadline =
        std::chrono::steady_clock::now() +
        std::chrono::seconds(5);

    while (manager.watcher_count() != 0 &&
           std::chrono::steady_clock::now() < deadline)
    {
        const auto completion =
            iocp.wait(std::chrono::milliseconds(100));

        if (!completion)
        {
            continue;
        }

        if (!manager.handle_completion(*completion))
		{
			continue;
		}
    }

    EXPECT_EQ(manager.watcher_count(), 0u);
}

void stop_manager(
    cwm::filesystem::WatchManager& manager,
    cwm::iocp::IocpContext& iocp)
{
    manager.stop();

    const auto deadline =
        std::chrono::steady_clock::now() +
        std::chrono::seconds(5);

    while (manager.watcher_count() != 0 &&
           std::chrono::steady_clock::now() < deadline)
    {
        const auto completion =
            iocp.wait(std::chrono::milliseconds(100));

        if (!completion)
        {
            continue;
        }

		if (!manager.handle_completion(*completion))
		{
			continue;
		}
    }

    ASSERT_EQ(manager.watcher_count(), 0u);
}

TEST_F(WatchManagerTest, ManagesMultipleDirectories)
{
    cwm::iocp::IocpContext iocp;

    cwm::concurrency::BoundedQueue<cwm::filesystem::FileSystemEvent>
        event_queue(32);

    cwm::filesystem::WatchManager manager(iocp, event_queue);

    const auto directory_a =
        create_directory(test_root_, "directory_a");

    const auto directory_b =
        create_directory(test_root_, "directory_b");

    const auto directory_c =
        create_directory(test_root_, "directory_c");

    manager.add_directory(directory_a);
    manager.add_directory(directory_b);
    manager.add_directory(directory_c);

    ASSERT_EQ(manager.watcher_count(), 3u);

    stop_manager(manager, iocp);
}

void stop_and_drain(
    cwm::filesystem::WatchManager& manager,
    cwm::iocp::IocpContext& iocp)
{
    manager.stop();

    const auto deadline =
        std::chrono::steady_clock::now() +
        std::chrono::seconds(5);

    while (manager.watcher_count() != 0 &&
           std::chrono::steady_clock::now() < deadline)
    {
        const auto completion =
            iocp.wait(std::chrono::milliseconds(100));

        if (!completion)
        {
            continue;
        }

        if (!manager.handle_completion(*completion))
		{
			continue;
		}
    }

    ASSERT_EQ(manager.watcher_count(), 0u);
}


TEST_F(WatchManagerTest, RoutesCompletionToCorrectWatcher)
{
    cwm::iocp::IocpContext iocp;

    cwm::concurrency::BoundedQueue<cwm::filesystem::FileSystemEvent>
        event_queue(16);

    cwm::filesystem::WatchManager manager(iocp, event_queue);

    const auto directory =
        create_directory(test_root_, "directory");

    manager.add_directory(directory);

    const auto file =
        directory / "created.txt";

    create_file(file);

    bool event_received = false;

    const auto deadline =
        std::chrono::steady_clock::now() +
        std::chrono::seconds(5);

    while (std::chrono::steady_clock::now() < deadline)
    {
        const auto completion =
            iocp.wait(std::chrono::milliseconds(100));

        if (!completion)
        {
            continue;
        }

        ASSERT_TRUE(
            manager.handle_completion(*completion));

        std::stop_source stop_source;
        stop_source.request_stop();

        auto event =
            event_queue.wait_pop(stop_source.get_token());

        if (!event)
        {
            continue;
        }

        if (event->action ==
                cwm::filesystem::FileSystemEventAction::Added &&
            event->path == file)
        {
            event_received = true;
            break;
        }
    }

    EXPECT_TRUE(event_received);

    stop_and_drain(manager, iocp);
}



TEST_F(WatchManagerTest, RemoveDirectoryStopsWatcher)
{
    cwm::iocp::IocpContext iocp;

    cwm::concurrency::BoundedQueue<cwm::filesystem::FileSystemEvent>
        event_queue(16);

    cwm::filesystem::WatchManager manager(iocp, event_queue);

    const auto directory =
        create_directory(test_root_, "directory");

    manager.add_directory(directory);

    ASSERT_EQ(manager.watcher_count(), 1u);

    manager.remove_directory(directory);

    const auto deadline =
        std::chrono::steady_clock::now() +
        std::chrono::seconds(5);

    while (manager.watcher_count() != 0 &&
           std::chrono::steady_clock::now() < deadline)
    {
        const auto completion =
            iocp.wait(std::chrono::milliseconds(100));

        if (!completion)
        {
            continue;
        }

        if (!manager.handle_completion(*completion)) {
			continue;
		}
    }

    EXPECT_EQ(manager.watcher_count(), 0u);
}

TEST_F(WatchManagerTest, StopStopsAllWatchers)
{
    cwm::iocp::IocpContext iocp;

    cwm::concurrency::BoundedQueue<cwm::filesystem::FileSystemEvent>
        event_queue(32);

    cwm::filesystem::WatchManager manager(iocp, event_queue);

    const auto directory_a =
        create_directory(test_root_, "directory_a");

    const auto directory_b =
        create_directory(test_root_, "directory_b");

    const auto directory_c =
        create_directory(test_root_, "directory_c");

    manager.add_directory(directory_a);
    manager.add_directory(directory_b);
    manager.add_directory(directory_c);

    ASSERT_EQ(manager.watcher_count(), 3u);

    manager.stop();

    const auto deadline =
        std::chrono::steady_clock::now() +
        std::chrono::seconds(5);

    while (manager.watcher_count() != 0 &&
           std::chrono::steady_clock::now() < deadline)
    {
        const auto completion =
            iocp.wait(std::chrono::milliseconds(100));

        if (!completion)
        {
            continue;
        }

        if (!manager.handle_completion(*completion)) {
			continue;
		}
    }

    EXPECT_EQ(manager.watcher_count(), 0u);
}


TEST_F(WatchManagerTest, RejectsDuplicateDirectory)
{
    cwm::iocp::IocpContext iocp;

    cwm::concurrency::BoundedQueue<cwm::filesystem::FileSystemEvent>
        event_queue(16);

    cwm::filesystem::WatchManager manager(iocp, event_queue);

    const auto directory =
        create_directory(test_root_, "directory");

    manager.add_directory(directory);

    ASSERT_EQ(manager.watcher_count(), 1u);

    EXPECT_THROW(
        manager.add_directory(directory),
        std::logic_error);

    EXPECT_EQ(manager.watcher_count(), 1u);

    stop_and_drain(manager, iocp);
}


TEST_F(WatchManagerTest, RemovingUnknownDirectoryDoesNothing)
{
    cwm::iocp::IocpContext iocp;

    cwm::concurrency::BoundedQueue<cwm::filesystem::FileSystemEvent>
        event_queue(16);

    cwm::filesystem::WatchManager manager(iocp, event_queue);

    const auto directory =
        create_directory(test_root_, "directory");

    EXPECT_NO_THROW(manager.remove_directory(directory));

    EXPECT_EQ(manager.watcher_count(), 0u);
}

} // namespace


TEST_F(WatchManagerTest, AddDirectoryStartsWatchersForExistingSubdirectories)
{
    cwm::iocp::IocpContext iocp;

    cwm::concurrency::BoundedQueue<cwm::filesystem::FileSystemEvent>
        event_queue(32);

    cwm::filesystem::WatchManager manager(iocp, event_queue);

    const auto root =
        create_directory(test_root_, "root");

    create_directory(root, "directory_a");
    create_directory(root, "directory_b");
    create_directory(root, "directory_c");

    manager.add_directory(root);

    EXPECT_EQ(manager.watcher_count(), 4u);

    stop_and_drain(manager, iocp);
}

TEST_F(WatchManagerTest, AddDirectoryIgnoresFiles)
{
    cwm::iocp::IocpContext iocp;

    cwm::concurrency::BoundedQueue<cwm::filesystem::FileSystemEvent>
        event_queue(16);

    cwm::filesystem::WatchManager manager(iocp, event_queue);

    const auto root =
        create_directory(test_root_, "root");

    create_directory(root, "subdirectory");

    create_file(root / "file.txt");

    create_file(root / "another_file.txt");

    manager.add_directory(root);

    EXPECT_EQ(manager.watcher_count(), 2u);

    stop_and_drain(manager, iocp);
}

TEST_F(WatchManagerTest, AddDirectoryHandlesEmptyDirectory)
{
    cwm::iocp::IocpContext iocp;

    cwm::concurrency::BoundedQueue<cwm::filesystem::FileSystemEvent>
        event_queue(16);

    cwm::filesystem::WatchManager manager(iocp, event_queue);

    const auto root =
        create_directory(test_root_, "root");

    manager.add_directory(root);

    EXPECT_EQ(manager.watcher_count(), 1u);

    stop_and_drain(manager, iocp);
}


TEST_F(WatchManagerTest, StartsWatcherForNewDirectory)
{
    cwm::iocp::IocpContext iocp;

    cwm::concurrency::BoundedQueue<cwm::filesystem::FileSystemEvent>
        event_queue(32);

    cwm::filesystem::WatchManager manager(iocp, event_queue);

    const auto root =
        create_directory(test_root_, "root");

    manager.add_directory(root);

    ASSERT_EQ(manager.watcher_count(), 1u);

    const auto new_directory =
        root / "new_directory";

    ASSERT_TRUE(
        std::filesystem::create_directory(new_directory));

    bool watcher_started = false;

    const auto deadline =
        std::chrono::steady_clock::now() +
        std::chrono::seconds(5);

    while (std::chrono::steady_clock::now() < deadline)
    {
        const auto completion =
            iocp.wait(std::chrono::milliseconds(100));

        if (!completion)
        {
            continue;
        }

        //manager.handle_completion(*completion);
		if (!manager.handle_completion(*completion)) {
			continue;
		}

        if (manager.watcher_count() == 2u)
        {
            watcher_started = true;
            break;
        }
    }

    EXPECT_TRUE(watcher_started);
    EXPECT_EQ(manager.watcher_count(), 2u);

    stop_and_drain(manager, iocp);
}

TEST_F(
    WatchManagerTest,
    ReconcilesDirectoryAfterWatcherFailure)
{
    cwm::iocp::IocpContext iocp;

    cwm::concurrency::BoundedQueue<
        cwm::filesystem::FileSystemEvent>
        event_queue(1);

    cwm::filesystem::WatchManagerConfig config;
    config.deferred_event_capacity = 1;

    cwm::filesystem::WatchManager manager(
        iocp,
        event_queue,
        config);

    const auto root =
        create_directory(test_root_, "root");

    const auto child =
        create_directory(root, "child");

    manager.add_directory(root);

    ASSERT_EQ(
        manager.watcher_count(),
        2u);

    // Occupy the event queue so filesystem events
    // must be deferred by the watcher.
    cwm::filesystem::FileSystemEvent queued_event{
        root / "queued.txt",
        cwm::filesystem::FileSystemEventAction::Added,
        std::chrono::steady_clock::now()};

    ASSERT_EQ(
        event_queue.try_push(queued_event),
        cwm::concurrency::QueuePushResult::Accepted);

    // Generate enough events to overflow the deferred
    // event capacity of the root watcher.
    const auto file1 =
        root / "overflow_1.txt";

    const auto file2 =
        root / "overflow_2.txt";

    create_file(file1);
    create_file(file2);

    bool reconciliation_triggered = false;

    const auto deadline =
        std::chrono::steady_clock::now() +
        std::chrono::seconds(5);

    while (std::chrono::steady_clock::now() < deadline)
    {
        const auto completion =
            iocp.wait(std::chrono::milliseconds(100));

        if (!completion)
        {
            continue;
        }

		if (!manager.handle_completion(*completion)) {
			continue;
		}

        /*
         * During reconciliation the affected watcher is
         * temporarily removed. We therefore don't require
         * watcher_count() to remain 2 at every instant.
         */
        if (manager.watcher_count() == 2u)
        {
            reconciliation_triggered = true;
            break;
        }
    }

    EXPECT_TRUE(reconciliation_triggered);
    EXPECT_EQ(
        manager.watcher_count(),
        2u);

    stop_and_drain(manager, iocp);
}

TEST_F(
    WatchManagerTest,
    ReconcilesDirectoryAfterDeferredEventOverflow)
{
    cwm::iocp::IocpContext iocp;

    cwm::concurrency::BoundedQueue<
        cwm::filesystem::FileSystemEvent>
        event_queue(1);

    cwm::filesystem::WatchManagerConfig config;
    config.deferred_event_capacity = 1;

    cwm::filesystem::WatchManager manager(
        iocp,
        event_queue,
        config);

    const auto root =
        create_directory(test_root_, "root");

    const auto child =
        create_directory(root, "child");

    manager.add_directory(root);

    ASSERT_EQ(
        manager.watcher_count(),
        2u);

    cwm::filesystem::FileSystemEvent queued_event{
        root / "queued.txt",
        cwm::filesystem::FileSystemEventAction::Added,
        std::chrono::steady_clock::now()};

    ASSERT_EQ(
        event_queue.try_push(queued_event),
        cwm::concurrency::QueuePushResult::Accepted);

    create_file(root / "overflow_1.txt");
    create_file(root / "overflow_2.txt");

    bool watcher_reconciled = false;

    const auto deadline =
        std::chrono::steady_clock::now() +
        std::chrono::seconds(5);

    while (std::chrono::steady_clock::now() < deadline)
    {
        const auto completion =
            iocp.wait(std::chrono::milliseconds(100));

        if (!completion)
        {
            continue;
        }

        if (!manager.handle_completion(*completion)) {
			continue;
		}

        if (manager.watcher_count() == 2u)
        {
            watcher_reconciled = true;
            break;
        }
    }

    ASSERT_TRUE(watcher_reconciled);
    ASSERT_EQ(
        manager.watcher_count(),
        2u);

    // Verify that the recreated root watcher is functional.
    const auto post_reconciliation_directory =
        root / "post_reconciliation";

    ASSERT_TRUE(
        std::filesystem::create_directory(
            post_reconciliation_directory));

    bool new_watcher_started = false;

    const auto watcher_deadline =
        std::chrono::steady_clock::now() +
        std::chrono::seconds(5);

    while (std::chrono::steady_clock::now() < watcher_deadline)
    {
        const auto completion =
            iocp.wait(std::chrono::milliseconds(100));

        if (!completion)
        {
            continue;
        }

        if (!manager.handle_completion(*completion)) {
			continue;
		}

        if (manager.watcher_count() == 3u)
        {
            new_watcher_started = true;
            break;
        }
    }

    EXPECT_TRUE(new_watcher_started);
    EXPECT_EQ(
        manager.watcher_count(),
        3u);

    stop_and_drain(manager, iocp);
}
