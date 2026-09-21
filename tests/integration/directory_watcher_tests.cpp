#include <cwm/concurrency/bounded_queue.h>
#include <cwm/filesystem/directory_watcher.h>
#include <cwm/filesystem/file_system_event.h>
#include <cwm/iocp/iocp_context.h>

#include <Windows.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

#include <gtest/gtest.h>

namespace
{

class DirectoryWatcherTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        const auto base =
            std::filesystem::temp_directory_path();

        test_directory_ =
            base / ("cwm-directory-watcher-" +
                    std::to_string(
                        GetCurrentProcessId()) +
                    "-" +
                    std::to_string(
                        GetCurrentThreadId()));

        std::filesystem::create_directories(
            test_directory_);
    }

    void TearDown() override
    {
        std::error_code error;

        std::filesystem::remove_all(
            test_directory_,
            error);
    }

    std::filesystem::path test_directory_;
};

} // namespace

TEST_F(
    DirectoryWatcherTest,
    DetectsFileCreation)
{
    cwm::iocp::IocpContext iocp;

    cwm::concurrency::BoundedQueue<
        cwm::filesystem::FileSystemEvent>
        event_queue(16);

    constexpr ULONG_PTR completion_key = 1234;

    cwm::filesystem::DirectoryWatcherConfig config;

    config.directory = test_directory_;
    config.completion_key = completion_key;

    cwm::filesystem::DirectoryWatcher watcher(
        iocp,
        event_queue,
        config);

    watcher.start();

    const auto file_path =
        test_directory_ / "created.txt";

    {
        std::ofstream file(file_path);

        ASSERT_TRUE(file.is_open());

        file << "Hello IOCP!";
    }

    bool event_received = false;

    constexpr auto timeout =
        std::chrono::seconds(2);

    const auto deadline =
        std::chrono::steady_clock::now() +
        timeout;

	while (std::chrono::steady_clock::now() < deadline)
	{
		const auto remaining =
			std::chrono::duration_cast<std::chrono::milliseconds>(
				deadline - std::chrono::steady_clock::now());

		const auto completion = iocp.wait(remaining);

		if (!completion.has_value())
		{
			break;
		}

		if (!watcher.handle_completion(*completion))
		{
			continue;
		}

		/*		
		const bool handled =
		watcher.handle_completion(*completion);

		ASSERT_TRUE(handled)
			<< "Unexpected IOCP completion: "
			<< "bytes=" << completion->bytes_transferred
			<< ", key=" << completion->completion_key
			<< ", error=" << completion->error;
		*/
	
		//ASSERT_EQ(
		//watcher.state(),
		//cwm::filesystem::DirectoryWatcherState::Running);

		while (event_queue.size() > 0)
		{
			auto event =
				event_queue.wait_pop(std::stop_token{});

			ASSERT_TRUE(event.has_value());

			if (event->action ==
					cwm::filesystem::
						FileSystemEventAction::Added &&
				event->path == file_path)
			{
				event_received = true;
				break;
			}
		}

		if (event_received)
		{
			break;
		}
	}

    EXPECT_TRUE(event_received);

	watcher.stop();

	bool stopped = false;

	const auto stop_deadline =
		std::chrono::steady_clock::now() +
		std::chrono::seconds(2);

	while (std::chrono::steady_clock::now() <
		   stop_deadline)
	{
		const auto remaining =
			std::chrono::duration_cast<std::chrono::milliseconds>(
				stop_deadline -
				std::chrono::steady_clock::now());

		const auto completion = iocp.wait(remaining);

		if (!completion.has_value())
		{
			break;
		}

		ASSERT_TRUE(
		watcher.handle_completion(*completion));
		
		/*		
		const bool handled =
		watcher.handle_completion(*completion);

		ASSERT_TRUE(handled)
			<< "Shutdown completion: "
			<< "bytes=" << completion->bytes_transferred
			<< ", key=" << completion->completion_key
			<< ", error=" << completion->error
			<< ", operation=" << completion->operation;
			
			std::cerr
			<< "Shutdown completion: "
			<< "bytes=" << completion->bytes_transferred
			<< ", key=" << completion->completion_key
			<< ", error=" << completion->error
			<< ", operation=" << completion->operation
			<< '\n';
		*/

		if (watcher.state() ==
			cwm::filesystem::DirectoryWatcherState::Stopped)
		{
			stopped = true;
			break;
		}
	}

	EXPECT_TRUE(stopped);
    EXPECT_EQ(
        watcher.state(),
        cwm::filesystem::
            DirectoryWatcherState::Stopped);
}


TEST_F(
    DirectoryWatcherTest,
    DetectsMultipleFileCreations)
{
    cwm::iocp::IocpContext iocp;

    cwm::concurrency::BoundedQueue<
        cwm::filesystem::FileSystemEvent>
        event_queue(16);

    constexpr ULONG_PTR completion_key = 1235;

    cwm::filesystem::DirectoryWatcherConfig config;
    config.directory = test_directory_;
    config.completion_key = completion_key;

    cwm::filesystem::DirectoryWatcher watcher(
        iocp,
        event_queue,
        config);

    watcher.start();

    const auto file_a =
        test_directory_ / "file_a.txt";

    const auto file_b =
        test_directory_ / "file_b.txt";

    const auto file_c =
        test_directory_ / "file_c.txt";

    {
        std::ofstream file(file_a);
        ASSERT_TRUE(file.is_open());
        file << "A";
    }

    {
        std::ofstream file(file_b);
        ASSERT_TRUE(file.is_open());
        file << "B";
    }

    {
        std::ofstream file(file_c);
        ASSERT_TRUE(file.is_open());
        file << "C";
    }

    bool received_a = false;
    bool received_b = false;
    bool received_c = false;

    constexpr auto timeout =
        std::chrono::seconds(2);

    const auto deadline =
        std::chrono::steady_clock::now() +
        timeout;

    while (std::chrono::steady_clock::now() < deadline &&
           !(received_a && received_b && received_c))
    {
        const auto remaining =
            std::chrono::duration_cast<
                std::chrono::milliseconds>(
                deadline -
                std::chrono::steady_clock::now());

        const auto completion =
            iocp.wait(remaining);

        if (!completion.has_value())
        {
            break;
        }

        const bool handled =
            watcher.handle_completion(*completion);

        ASSERT_TRUE(handled)
            << "Unexpected IOCP completion: "
            << "bytes="
            << completion->bytes_transferred
            << ", key="
            << completion->completion_key
            << ", error="
            << completion->error;

        while (event_queue.size() > 0)
        {
            auto event =
                event_queue.wait_pop(
                    std::stop_token{});

            ASSERT_TRUE(event.has_value());

            if (event->action !=
                cwm::filesystem::
                    FileSystemEventAction::Added)
            {
                continue;
            }

            if (event->path == file_a)
            {
                received_a = true;
            }
            else if (event->path == file_b)
            {
                received_b = true;
            }
            else if (event->path == file_c)
            {
                received_c = true;
            }
        }
    }

    EXPECT_TRUE(received_a);
    EXPECT_TRUE(received_b);
    EXPECT_TRUE(received_c);

    watcher.stop();

    bool stopped = false;

    const auto stop_deadline =
        std::chrono::steady_clock::now() +
        std::chrono::seconds(2);

    while (std::chrono::steady_clock::now() <
           stop_deadline)
    {
        const auto remaining =
            std::chrono::duration_cast<
                std::chrono::milliseconds>(
                stop_deadline -
                std::chrono::steady_clock::now());

        const auto completion =
            iocp.wait(remaining);

        if (!completion.has_value())
        {
            break;
        }

        ASSERT_TRUE(
            watcher.handle_completion(*completion));

        if (watcher.state() ==
            cwm::filesystem::
                DirectoryWatcherState::Stopped)
        {
            stopped = true;
            break;
        }
    }

    EXPECT_TRUE(stopped);
    EXPECT_EQ(
        watcher.state(),
        cwm::filesystem::
            DirectoryWatcherState::Stopped);
}


TEST_F(
    DirectoryWatcherTest,
    CanRestartAfterStop)
{
    cwm::iocp::IocpContext iocp;

    cwm::concurrency::BoundedQueue<
        cwm::filesystem::FileSystemEvent>
        event_queue(16);

    constexpr ULONG_PTR completion_key = 1236;

    cwm::filesystem::DirectoryWatcherConfig config;
    config.directory = test_directory_;
    config.completion_key = completion_key;

    cwm::filesystem::DirectoryWatcher watcher(
        iocp,
        event_queue,
        config);

    //
    // First monitoring cycle.
    //
    watcher.start();

    const auto first_file =
        test_directory_ / "first.txt";

    {
        std::ofstream file(first_file);
        ASSERT_TRUE(file.is_open());
        file << "first";
    }

    bool first_event_received = false;

    const auto first_deadline =
        std::chrono::steady_clock::now() +
        std::chrono::seconds(2);

    while (std::chrono::steady_clock::now() <
           first_deadline)
    {
        const auto remaining =
            std::chrono::duration_cast<
                std::chrono::milliseconds>(
                first_deadline -
                std::chrono::steady_clock::now());

        const auto completion =
            iocp.wait(remaining);

        if (!completion.has_value())
        {
            break;
        }

        ASSERT_TRUE(
            watcher.handle_completion(*completion));

        while (event_queue.size() > 0)
        {
            auto event =
                event_queue.wait_pop(
                    std::stop_token{});

            ASSERT_TRUE(event.has_value());

            if (event->action ==
                    cwm::filesystem::
                        FileSystemEventAction::Added &&
                event->path == first_file)
            {
                first_event_received = true;
                break;
            }
        }

        if (first_event_received)
        {
            break;
        }
    }

    EXPECT_TRUE(first_event_received);

    //
    // Stop first monitoring cycle.
    //
    watcher.stop();

    bool stopped = false;

    const auto first_stop_deadline =
        std::chrono::steady_clock::now() +
        std::chrono::seconds(2);

    while (std::chrono::steady_clock::now() <
           first_stop_deadline)
    {
        const auto remaining =
            std::chrono::duration_cast<
                std::chrono::milliseconds>(
                first_stop_deadline -
                std::chrono::steady_clock::now());

        const auto completion =
            iocp.wait(remaining);

        if (!completion.has_value())
        {
            break;
        }

        ASSERT_TRUE(
            watcher.handle_completion(*completion));

        if (watcher.state() ==
            cwm::filesystem::
                DirectoryWatcherState::Stopped)
        {
            stopped = true;
            break;
        }
    }

    ASSERT_TRUE(stopped);
    ASSERT_EQ(
        watcher.state(),
        cwm::filesystem::
            DirectoryWatcherState::Stopped);

    //
    // Second monitoring cycle.
    //
    watcher.start();

    ASSERT_EQ(
        watcher.state(),
        cwm::filesystem::
            DirectoryWatcherState::Running);

    const auto second_file =
        test_directory_ / "second.txt";

    {
        std::ofstream file(second_file);
        ASSERT_TRUE(file.is_open());
        file << "second";
    }

    bool second_event_received = false;

    const auto second_deadline =
        std::chrono::steady_clock::now() +
        std::chrono::seconds(2);

    while (std::chrono::steady_clock::now() <
           second_deadline)
    {
        const auto remaining =
            std::chrono::duration_cast<
                std::chrono::milliseconds>(
                second_deadline -
                std::chrono::steady_clock::now());

        const auto completion =
            iocp.wait(remaining);

        if (!completion.has_value())
        {
            break;
        }

        ASSERT_TRUE(
            watcher.handle_completion(*completion));

        while (event_queue.size() > 0)
        {
            auto event =
                event_queue.wait_pop(
                    std::stop_token{});

            ASSERT_TRUE(event.has_value());

            if (event->action ==
                    cwm::filesystem::
                        FileSystemEventAction::Added &&
                event->path == second_file)
            {
                second_event_received = true;
                break;
            }
        }

        if (second_event_received)
        {
            break;
        }
    }

    EXPECT_TRUE(second_event_received);

    //
    // Final shutdown.
    //
    watcher.stop();

    stopped = false;

    const auto second_stop_deadline =
        std::chrono::steady_clock::now() +
        std::chrono::seconds(2);

    while (std::chrono::steady_clock::now() <
           second_stop_deadline)
    {
        const auto remaining =
            std::chrono::duration_cast<
                std::chrono::milliseconds>(
                second_stop_deadline -
                std::chrono::steady_clock::now());

        const auto completion =
            iocp.wait(remaining);

        if (!completion.has_value())
        {
            break;
        }

        ASSERT_TRUE(
            watcher.handle_completion(*completion));

        if (watcher.state() ==
            cwm::filesystem::
                DirectoryWatcherState::Stopped)
        {
            stopped = true;
            break;
        }
    }

    EXPECT_TRUE(stopped);
    EXPECT_EQ(
        watcher.state(),
        cwm::filesystem::
            DirectoryWatcherState::Stopped);
}


TEST_F(
    DirectoryWatcherTest,
    DefersEventsWhenEventQueueIsFull)
{
    cwm::iocp::IocpContext iocp;

    // Deliberately tiny queue.
    cwm::concurrency::BoundedQueue<
        cwm::filesystem::FileSystemEvent>
        event_queue(1);

    constexpr ULONG_PTR completion_key = 1237;

    cwm::filesystem::DirectoryWatcherConfig config;
    config.directory = test_directory_;
    config.completion_key = completion_key;
    config.deferred_event_capacity = 4;

    cwm::filesystem::DirectoryWatcher watcher(
        iocp,
        event_queue,
        config);

    watcher.start();

    const auto file_a =
        test_directory_ / "backpressure_a.txt";

    const auto file_b =
        test_directory_ / "backpressure_b.txt";

    // Fill the external queue first.
    cwm::filesystem::FileSystemEvent queued_event{
        test_directory_ / "already_queued.txt",
        cwm::filesystem::FileSystemEventAction::Added,
        std::chrono::steady_clock::now()};

    EXPECT_EQ(
        event_queue.try_push(queued_event),
        cwm::concurrency::QueuePushResult::Accepted);

    // Generate two real filesystem events.
    {
        std::ofstream file(file_a);
        ASSERT_TRUE(file.is_open());
        file << "A";
    }

    {
        std::ofstream file(file_b);
        ASSERT_TRUE(file.is_open());
        file << "B";
    }

    const auto deadline =
        std::chrono::steady_clock::now() +
        std::chrono::seconds(2);

    while (std::chrono::steady_clock::now() < deadline)
    {
        const auto remaining =
            std::chrono::duration_cast<
                std::chrono::milliseconds>(
                deadline -
                std::chrono::steady_clock::now());

        const auto completion =
            iocp.wait(remaining);

        if (!completion.has_value())
        {
            break;
        }

        ASSERT_TRUE(
            watcher.handle_completion(*completion));

        // We expect at least one event to have been deferred.
        if (watcher.deferred_event_count() > 0)
        {
            break;
        }
    }

    EXPECT_GT(
        watcher.deferred_event_count(),
        0u);

    EXPECT_EQ(
        watcher.state(),
        cwm::filesystem::
            DirectoryWatcherState::Running);

    // Clean up the queued event.
    auto queued =
        event_queue.wait_pop(std::stop_token{});

    ASSERT_TRUE(queued.has_value());

    watcher.stop();

    bool stopped = false;

    const auto stop_deadline =
        std::chrono::steady_clock::now() +
        std::chrono::seconds(2);

    while (std::chrono::steady_clock::now() <
           stop_deadline)
    {
        const auto remaining =
            std::chrono::duration_cast<
                std::chrono::milliseconds>(
                stop_deadline -
                std::chrono::steady_clock::now());

        const auto completion =
            iocp.wait(remaining);

        if (!completion.has_value())
        {
            break;
        }

        ASSERT_TRUE(
            watcher.handle_completion(*completion));

        if (watcher.state() ==
            cwm::filesystem::
                DirectoryWatcherState::Stopped)
        {
            stopped = true;
            break;
        }
    }

    EXPECT_TRUE(stopped);
}


TEST_F(
    DirectoryWatcherTest,
    RetryDeferredEventsMovesEventsToQueue)
{
    cwm::iocp::IocpContext iocp;

    cwm::concurrency::BoundedQueue<
        cwm::filesystem::FileSystemEvent>
        event_queue(1);

    constexpr ULONG_PTR completion_key = 1238;

    cwm::filesystem::DirectoryWatcherConfig config;
    config.directory = test_directory_;
    config.completion_key = completion_key;
    config.deferred_event_capacity = 4;

    cwm::filesystem::DirectoryWatcher watcher(
        iocp,
        event_queue,
        config);

    watcher.start();

    // Occupy the queue so filesystem events have to
    // go into the deferred cache.
    cwm::filesystem::FileSystemEvent queued_event{
        test_directory_ / "queued.txt",
        cwm::filesystem::FileSystemEventAction::Added,
        std::chrono::steady_clock::now()};

    EXPECT_EQ(
        event_queue.try_push(queued_event),
        cwm::concurrency::QueuePushResult::Accepted);

    const auto deferred_file =
        test_directory_ / "deferred.txt";

    {
        std::ofstream file(deferred_file);
        ASSERT_TRUE(file.is_open());
        file << "deferred";
    }

    const auto deadline =
        std::chrono::steady_clock::now() +
        std::chrono::seconds(2);

    while (std::chrono::steady_clock::now() < deadline)
    {
        const auto remaining =
            std::chrono::duration_cast<
                std::chrono::milliseconds>(
                deadline -
                std::chrono::steady_clock::now());

        const auto completion =
            iocp.wait(remaining);

        if (!completion.has_value())
        {
            break;
        }

        ASSERT_TRUE(
            watcher.handle_completion(*completion));

        if (watcher.deferred_event_count() > 0)
        {
            break;
        }
    }


	ASSERT_GE(
		watcher.deferred_event_count(),
		1u);

	// Make room in the external queue.
	auto queued =
		event_queue.wait_pop(std::stop_token{});

	ASSERT_TRUE(queued.has_value());

	std::size_t total_accepted = 0;
	bool deferred_event_found = false;

	while (watcher.deferred_event_count() > 0)
	{
		const auto accepted =
			watcher.retry_deferred_events();

		ASSERT_GT(accepted, 0u);

		total_accepted += accepted;

		// The queue capacity is one, so consume the event
		// before attempting another retry.
		auto event =
			event_queue.wait_pop(std::stop_token{});

		ASSERT_TRUE(event.has_value());

		if (event->path == deferred_file &&
			event->action ==
				cwm::filesystem::FileSystemEventAction::Added)
		{
			deferred_event_found = true;
		}
	}

	EXPECT_GE(total_accepted, 1u);
	EXPECT_TRUE(deferred_event_found);

	EXPECT_EQ(
		watcher.deferred_event_count(),
		0u);

	watcher.stop();



    bool stopped = false;

    const auto stop_deadline =
        std::chrono::steady_clock::now() +
        std::chrono::seconds(2);

    while (std::chrono::steady_clock::now() <
           stop_deadline)
    {
        const auto remaining =
            std::chrono::duration_cast<
                std::chrono::milliseconds>(
                stop_deadline -
                std::chrono::steady_clock::now());

        const auto completion =
            iocp.wait(remaining);

        if (!completion.has_value())
        {
            break;
        }

        ASSERT_TRUE(
            watcher.handle_completion(*completion));

        if (watcher.state() ==
            cwm::filesystem::
                DirectoryWatcherState::Stopped)
        {
            stopped = true;
            break;
        }
    }

    EXPECT_TRUE(stopped);
}


TEST_F(
    DirectoryWatcherTest,
    DeferredEventOverflowRequiresReconciliation)
{
    cwm::iocp::IocpContext iocp;

    // One event in the external queue.
    cwm::concurrency::BoundedQueue<
        cwm::filesystem::FileSystemEvent>
        event_queue(1);

    constexpr ULONG_PTR completion_key = 1239;

    cwm::filesystem::DirectoryWatcherConfig config;
    config.directory = test_directory_;
    config.completion_key = completion_key;

    // Only one event can be retained locally.
    config.deferred_event_capacity = 1;

    cwm::filesystem::DirectoryWatcher watcher(
        iocp,
        event_queue,
        config);

    watcher.start();

    // Fill the external queue.
    cwm::filesystem::FileSystemEvent queued_event{
        test_directory_ / "queued.txt",
        cwm::filesystem::FileSystemEventAction::Added,
        std::chrono::steady_clock::now()};

    EXPECT_EQ(
        event_queue.try_push(queued_event),
        cwm::concurrency::QueuePushResult::Accepted);

    const auto file_a =
        test_directory_ / "overflow_a.txt";

    const auto file_b =
        test_directory_ / "overflow_b.txt";

    const auto file_c =
        test_directory_ / "overflow_c.txt";

    {
        std::ofstream file(file_a);
        ASSERT_TRUE(file.is_open());
        file << "A";
    }

    {
        std::ofstream file(file_b);
        ASSERT_TRUE(file.is_open());
        file << "B";
    }

    {
        std::ofstream file(file_c);
        ASSERT_TRUE(file.is_open());
        file << "C";
    }

    const auto deadline =
        std::chrono::steady_clock::now() +
        std::chrono::seconds(2);

    while (std::chrono::steady_clock::now() < deadline)
    {
        const auto remaining =
            std::chrono::duration_cast<
                std::chrono::milliseconds>(
                deadline -
                std::chrono::steady_clock::now());

        const auto completion =
            iocp.wait(remaining);

        if (!completion.has_value())
        {
            break;
        }

        ASSERT_TRUE(
            watcher.handle_completion(*completion));

        if (watcher.state() ==
            cwm::filesystem::
                DirectoryWatcherState::ReconciliationRequired)
        {
            break;
        }
    }

    EXPECT_EQ(
        watcher.state(),
        cwm::filesystem::
            DirectoryWatcherState::ReconciliationRequired);

    EXPECT_EQ(
        watcher.reconciliation_reason(),
        cwm::filesystem::
            ReconciliationReason::DeferredEventOverflow);

    EXPECT_EQ(
        watcher.deferred_event_count(),
        1u);

    // Remove the item occupying the external queue.
    auto queued =
        event_queue.wait_pop(std::stop_token{});

    ASSERT_TRUE(queued.has_value());

    watcher.stop();

    bool stopped = false;

    const auto stop_deadline =
        std::chrono::steady_clock::now() +
        std::chrono::seconds(2);

    while (std::chrono::steady_clock::now() <
           stop_deadline)
    {
        const auto remaining =
            std::chrono::duration_cast<
                std::chrono::milliseconds>(
                stop_deadline -
                std::chrono::steady_clock::now());

        const auto completion =
            iocp.wait(remaining);

        if (!completion.has_value())
        {
            break;
        }

        ASSERT_TRUE(
            watcher.handle_completion(*completion));

        if (watcher.state() ==
            cwm::filesystem::
                DirectoryWatcherState::Stopped)
        {
            stopped = true;
            break;
        }
    }

    EXPECT_TRUE(stopped);
}
