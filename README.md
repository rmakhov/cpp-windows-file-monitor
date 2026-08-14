# cpp-windows-file-monitor

**Production-oriented asynchronous Windows filesystem monitoring framework built with C++20, IOCP, and `ReadDirectoryChangesW`.**

> A reusable C++20 IOCP layer combined with a scalable Windows filesystem monitoring framework designed around concurrency, reliability, backpressure, graceful shutdown, and recovery from filesystem notification loss.

---

## Overview

`cpp-windows-file-monitor` is a modern C++20 project for monitoring Windows directories using asynchronous I/O and I/O Completion Ports (IOCP).

The project is intentionally designed as more than a simple `ReadDirectoryChangesW` example.

Its primary goal is to explore and demonstrate how to build a **reliable, scalable, and maintainable event-driven filesystem monitoring system** capable of monitoring many directories concurrently while handling real-world failure modes.

The project consists of two major parts:

1. **Reusable C++20 IOCP infrastructure**
2. **Production-oriented filesystem monitoring framework**

The filesystem monitor is built on top of the reusable IOCP layer rather than embedding IOCP-specific logic directly into the watcher.

---

## Why This Project?

Windows provides `ReadDirectoryChangesW` for receiving filesystem change notifications. It is powerful, but building a reliable monitoring system around it requires solving several non-trivial problems:

* asynchronous I/O
* `OVERLAPPED` lifetime management
* IOCP completion handling
* concurrent directory monitoring
* notification buffer management
* event normalization
* rename correlation
* event bursts
* backpressure
* queue overload
* notification loss
* directory reconciliation
* file stabilization
* cancellation
* graceful shutdown
* error handling
* observability

A simple example can demonstrate how to call `ReadDirectoryChangesW`.

A production-oriented system must additionally answer:

> **What happens when the filesystem changes faster than the application can process events?**

> **What happens when the notification buffer overflows?**

> **What happens when a directory disappears while an asynchronous operation is pending?**

> **What happens when the process shuts down while multiple I/O operations are outstanding?**

> **How do we detect and recover from events that may have been lost?**

These questions drive the architecture of this project.

---

## Design Goals

### 1. Modern C++

The project uses C++20 and emphasizes:

* RAII
* explicit ownership
* deterministic resource lifetime
* type safety
* `std::filesystem`
* `std::jthread`
* `std::stop_token`
* modern concurrency primitives
* exception-safe resource management
* minimal raw ownership
* clear separation of concerns

---

### 2. Reusable IOCP Layer

The IOCP implementation is designed as an independent infrastructure component.

The IOCP layer should not know that it is being used for filesystem monitoring.

Conceptually:

```text
                 +----------------------+
                 |      IOCP Layer      |
                 |                      |
                 |  IocpContext         |
                 |  IocpOperation       |
                 |  IocpWorker          |
                 +----------+-----------+
                            |
             +--------------+--------------+
             |                             |
             v                             v
     Filesystem Monitor              Future Users
     ReadDirectoryChangesW           Named Pipes
                                    Sockets
                                    Other Win32 I/O
```

This separation allows the IOCP layer to become reusable infrastructure rather than a filesystem-specific implementation detail.

---

### 3. Scalable Directory Monitoring

Multiple directories should share the same IOCP infrastructure.

The architecture should avoid creating one thread per directory.

Conceptually:

```text
                    IOCP
                     |
       +-------------+-------------+
       |             |             |
       v             v             v
   Directory A   Directory B   Directory N
       |             |             |
       +-------------+-------------+
                     |
                     v
              IOCP Worker Pool
```

The number of monitored directories should therefore be largely independent of the number of worker threads.

---

### 4. Reliability

Filesystem notifications are treated as **change notifications rather than a durable source of truth**.

The system must explicitly handle conditions where notifications can no longer be considered complete.

For example:

```text
ReadDirectoryChangesW
          |
          v
    notification
          |
          X
   buffer overflow
          |
          v
   state is uncertain
          |
          v
    reconciliation
          |
          v
  filesystem becomes
     consistent again
```

This distinction is fundamental to the architecture.

---

### 5. Backpressure

Filesystem activity can temporarily exceed the application's processing capacity.

The system therefore separates:

```text
event acquisition
       |
       v
event processing
```

and uses bounded queues where appropriate.

The architecture explicitly considers:

```text
producer rate > consumer rate
```

rather than allowing uncontrolled memory growth.

---

### 6. Graceful Shutdown

Shutdown is treated as part of the normal lifecycle rather than an exceptional situation.

The system should be able to:

* stop accepting new work
* cancel outstanding I/O
* wake worker threads
* stop processing new events
* drain or discard queued work according to policy
* release directory handles
* release IOCP resources
* terminate deterministically

The target is a shutdown sequence that is:

* deterministic
* idempotent
* thread-safe
* deadlock-free
* exception-safe

---

## Architecture

The high-level architecture is:

```text
                         Application
                              |
                              v
                    +-------------------+
                    |   MonitorService  |
                    |                   |
                    | lifecycle         |
                    | configuration     |
                    | observability     |
                    +---------+---------+
                              |
                              v
                    +-------------------+
                    |    WatchManager   |
                    |                   |
                    | add/remove        |
                    | directory watches |
                    +---------+---------+
                              |
             +----------------+----------------+
             |                |                |
             v                v                v
        +---------+      +---------+      +---------+
        | Watch A |      | Watch B |      | Watch N |
        +----+----+      +----+----+      +----+----+
             |                |                |
             +----------------+----------------+
                              |
                              v
                    +-------------------+
                    |       IOCP        |
                    |                   |
                    | completion port   |
                    | worker threads    |
                    +---------+---------+
                              |
                              v
                    +-------------------+
                    | Event Dispatcher  |
                    +---------+---------+
                              |
                              v
                    +-------------------+
                    |  Event Pipeline   |
                    |                   |
                    | normalize         |
                    | filter            |
                    | coalesce          |
                    | stabilize         |
                    +---------+---------+
                              |
                              v
                    +-------------------+
                    |   Bounded Queue   |
                    +---------+---------+
                              |
                              v
                    +-------------------+
                    |   Worker Pool     |
                    +---------+---------+
                              |
                              v
                    +-------------------+
                    |  Event Handlers   |
                    +-------------------+
```

---

## IOCP Layer

The IOCP layer provides reusable asynchronous I/O infrastructure.

The intended responsibilities include:

* creating and owning an IOCP
* associating Windows handles with the completion port
* running completion worker threads
* representing asynchronous operations
* safely managing `OVERLAPPED` lifetime
* dispatching completions
* handling cancellation
* supporting deterministic shutdown

Conceptually:

```text
Win32 asynchronous operation
            |
            v
        OVERLAPPED
            |
            v
           IOCP
            |
            v
    completion worker
            |
            v
     operation object
```

The IOCP layer should remain independent of filesystem-specific types.

---

## Filesystem Monitoring

The filesystem layer uses:

```text
ReadDirectoryChangesW
```

with asynchronous I/O and IOCP.

The basic lifecycle is:

```text
CreateFileW
    |
    v
CreateIoCompletionPort
    |
    v
ReadDirectoryChangesW
    |
    v
IOCP completion
    |
    v
FILE_NOTIFY_INFORMATION
    |
    v
event normalization
    |
    v
application event
    |
    v
issue next ReadDirectoryChangesW
```

The monitoring operation must be continuously re-armed.

A successful completion is not the end of monitoring.

---

## Event Model

Windows exposes filesystem changes through `FILE_NOTIFY_INFORMATION`.

The framework converts those Windows-specific notifications into a higher-level event model.

For example:

```cpp
enum class FileAction
{
    Added,
    Removed,
    Modified,
    RenamedOldName,
    RenamedNewName
};
```

The application-facing event can conceptually look like:

```cpp
struct FileSystemEvent
{
    FileAction action;
    std::filesystem::path path;

    WatchId watch_id;

    std::chrono::system_clock::time_point timestamp;
};
```

The exact API may evolve during implementation.

The important architectural principle is:

> Application code should not need to understand `FILE_NOTIFY_INFORMATION`, `OVERLAPPED`, or raw Win32 handles.

---

## Rename Handling

Windows reports renames as two notifications:

```text
FILE_ACTION_RENAMED_OLD_NAME
FILE_ACTION_RENAMED_NEW_NAME
```

The framework should correlate those notifications when possible and expose a higher-level rename event:

```cpp
struct FileRenameEvent
{
    std::filesystem::path old_path;
    std::filesystem::path new_path;
};
```

This prevents Windows-specific notification semantics from leaking into application code.

---

## File Stabilization

A filesystem notification does not necessarily mean that a file is ready for processing.

For example, an application may perform:

```text
create file
    |
    v
write 100 MB
    |
    v
write another 100 MB
    |
    v
close file
```

The monitor may receive multiple notifications while the file is still being written.

A production-oriented processing pipeline can therefore introduce a stabilization stage:

```text
FileSystemEvent
       |
       v
StabilizationPolicy
       |
       +---- accessible?
       +---- size stable?
       +---- timestamp stable?
       +---- retry?
       |
       v
ReadyForProcessing
```

The stabilization policy should remain configurable and independent from the low-level watcher.

---

## Backpressure

The system explicitly separates event acquisition from event processing.

```text
IOCP
 |
 v
Event Dispatcher
 |
 v
+----------------------+
| Bounded Event Queue  |
+----------+-----------+
           |
           v
      Worker Pool
```

If event production exceeds processing capacity:

```text
producer rate
      >
consumer rate
```

the framework must have a defined policy rather than allowing unlimited queue growth.

Possible strategies include:

* bounded queues
* event coalescing
* dropping redundant events
* retrying work
* triggering reconciliation
* persistent queues

The exact policy depends on the processing mode.

---

## Notification Loss and Reconciliation

One of the most important design requirements is handling filesystem notification loss.

`ReadDirectoryChangesW` notifications are not a persistent event log.

If the notification buffer overflows or the system reports that directory enumeration is required, the monitor can no longer assume that every filesystem change has been observed.

The recovery model is:

```text
notification failure
        |
        v
watch becomes inconsistent
        |
        v
directory reconciliation
        |
        v
enumerate actual filesystem state
        |
        v
compare / reconcile state
        |
        v
resume notifications
```

This allows the system to recover from transient notification loss instead of silently continuing with an incorrect view of the filesystem.

---

## Persistence

Persistence is intentionally separated from the core filesystem monitoring mechanism.

The basic framework can operate entirely in memory:

```text
Filesystem
    |
    v
Monitor
    |
    v
Event Queue
    |
    v
Application
```

An optional durable processing layer can provide:

```text
Filesystem
    |
    v
Monitor
    |
    v
Persistent Queue
    |
    v
Application
```

A future SQLite-backed implementation may use a state model such as:

```text
NEW
 |
 v
PROCESSING
 |
 v
DONE
```

with crash recovery:

```text
PROCESSING
     |
     | process crash
     v
  recovery
     |
     v
    NEW
```

The project will avoid claiming "exactly once" semantics unless the complete end-to-end processing contract actually guarantees them.

---

## Concurrency Model

The architecture intentionally separates I/O completion handling from expensive event processing.

### IOCP workers

IOCP worker threads should primarily:

* retrieve completions
* identify the associated operation
* validate completion state
* parse notification data
* produce normalized events
* re-arm directory monitoring

They should avoid expensive application work.

### Processing workers

Application worker threads can perform:

* file stabilization
* hashing
* database operations
* expensive filesystem inspection
* application callbacks
* other potentially slow operations

Conceptually:

```text
                    IOCP
                     |
        +------------+------------+
        |            |            |
        v            v            v
     IOCP #1      IOCP #2      IOCP #N
        |            |            |
        +------------+------------+
                     |
                     v
               Event Queue
                     |
          +----------+----------+
          |          |          |
          v          v          v
       Worker 1   Worker 2   Worker N
```

This prevents slow application processing from starving the I/O completion layer.

---

## Shutdown Model

Shutdown follows an explicit lifecycle.

```text
MonitorService::stop()
        |
        +-- stop accepting new watches
        |
        +-- stop new work
        |
        +-- cancel outstanding I/O
        |
        +-- wake IOCP workers
        |
        +-- stop processing workers
        |
        +-- release directory watches
        |
        +-- release IOCP
        |
        v
      STOPPED
```

C++20 facilities such as:

```cpp
std::jthread
std::stop_token
```

are used where appropriate.

Shutdown must be safe even when operations are concurrently completing.

---

## Observability

Production systems need visibility into their behavior.

The framework is designed to expose useful runtime statistics such as:

```text
directories monitored
active watches
notifications received
events generated
events processed
events dropped
events coalesced
queue depth
queue high-water mark
processing latency
processing failures
retry count
reconciliation count
buffer overflow count
IOCP errors
```

The exact metrics API will evolve with implementation.

The goal is to make system behavior measurable rather than relying exclusively on log messages.

---

## Error Handling

Errors are divided into several categories.

### Expected operational errors

Examples:

* directory deleted
* directory temporarily unavailable
* access denied
* file disappears between notification and processing
* transient sharing violations

These should be represented as normal runtime conditions where appropriate.

### Programming errors

Examples:

* violated invariants
* invalid object state
* incorrect ownership
* impossible state transitions

These should be treated differently from expected filesystem conditions.

### System-level failures

Examples:

* IOCP failure
* memory allocation failure
* unexpected Win32 errors

The framework should preserve useful error information and avoid silently swallowing failures.

---

## Project Structure

The repository is organized around architectural boundaries.

```text
cpp-windows-file-monitor/
│
├── README.md
├── LICENSE
├── CMakeLists.txt
├── CMakePresets.json
│
├── docs/
│   ├── architecture.md
│   ├── concurrency.md
│   ├── reliability.md
│   ├── error-handling.md
│   ├── performance.md
│   └── decisions/
│       ├── 001-iocp.md
│       ├── 002-event-pipeline.md
│       ├── 003-backpressure.md
│       └── 004-reconciliation.md
│
├── include/
│   └── cwm/
│       ├── iocp/
│       ├── filesystem/
│       ├── pipeline/
│       └── monitor/
│
├── src/
│   ├── iocp/
│   ├── filesystem/
│   ├── pipeline/
│   └── monitor/
│
├── examples/
│   └── basic_monitor/
│
├── tests/
│   ├── unit/
│   ├── integration/
│   ├── stress/
│   └── test_data/
│
├── benchmarks/
│
└── tools/
    └── file-monitor-cli/
```

The actual structure may evolve as implementation progresses.

---

## Build Requirements

### Operating System

Windows 10 or later.

The project is intended to work on modern Windows desktop and server versions supported by the C++20 toolchain.

### Compiler

Recommended:

* Visual Studio 2022
* MSVC
* C++20

### Build System

* CMake
* CMake presets

### Dependencies

The core library is intended to minimize external dependencies.

The initial implementation should rely primarily on:

* C++20 standard library
* Windows SDK
* Windows I/O Completion Ports
* `ReadDirectoryChangesW`

Optional dependencies may be introduced for testing, logging, or persistence.

---

## Building

Clone the repository:

```powershell
git clone https://github.com/<your-account>/cpp-windows-file-monitor.git
cd cpp-windows-file-monitor
```

Configure:

```powershell
cmake --preset windows-msvc
```

Build:

```powershell
cmake --build --preset windows-msvc-debug
```

Run tests:

```powershell
ctest --preset windows-msvc-debug
```

The exact presets and commands will be finalized with the initial build system.

---

## Example

The intended high-level API should eventually look approximately like:

```cpp
#include <cwm/monitor.h>

int main()
{
    cwm::Monitor monitor;

    monitor.add_watch(
        R"(C:\Data)",
        [](const cwm::FileSystemEvent& event)
        {
            // Process filesystem event.
        });

    monitor.start();

    // Application continues running...

    monitor.stop();
}
```

The public API will remain intentionally simple even though the underlying implementation is highly asynchronous.

---

## Testing Strategy

The project is intended to include several levels of testing.

### Unit tests

Focus on isolated components:

* event parsing
* event normalization
* rename correlation
* queue behavior
* state transitions
* error handling
* path processing

### Integration tests

Exercise real Windows filesystem behavior:

* file creation
* file deletion
* file modification
* rename
* directory creation
* directory removal
* multiple simultaneous watches

### Stress tests

Test behavior under high filesystem activity:

```text
100,000+
filesystem operations
```

and evaluate:

* event throughput
* CPU consumption
* memory consumption
* queue behavior
* event latency
* recovery behavior

### Failure tests

Explicitly test:

* notification buffer overflow
* directory deletion during monitoring
* access changes
* rapid create/delete cycles
* shutdown during active I/O
* cancellation during completion
* slow event consumers
* worker thread termination
* application restart

---

## Performance Goals

The project is not designed around a single arbitrary "events per second" number.

Instead, performance should be evaluated using measurable characteristics:

```text
Throughput
Latency
CPU utilization
Memory utilization
Queue depth
Recovery time
Scalability with number of watches
```

Representative benchmark scenarios will include:

### Low activity

```text
10 directories
10 events/sec
```

### Medium activity

```text
100 directories
1,000 events/sec
```

### High activity

```text
1,000+ directories
10,000+ events/sec
```

Actual supported limits will be determined empirically rather than claimed in advance.

---

## Reliability Model

The project intentionally distinguishes between:

### Notification stream

```text
ReadDirectoryChangesW
```

and:

### Filesystem state

```text
actual contents of the directory
```

The notification stream is used for efficient incremental processing.

The filesystem itself remains the source of truth.

When notification integrity is uncertain, reconciliation can restore consistency.

This leads to the following model:

```text
                 Filesystem State
                       ^
                       |
                 reconciliation
                       |
                       |
Notification Stream --> Event Pipeline
```

This is an important design principle of the project.

---

## Design Principles

The implementation follows several principles.

### Keep platform-specific code contained

Windows-specific APIs should remain below well-defined abstraction boundaries.

### Prefer ownership over convention

Object lifetime should be represented explicitly in types whenever practical.

### Make concurrency visible

Thread ownership, synchronization, and lifecycle should be clear from the architecture.

### Don't hide failure modes

Filesystem notification loss, queue overload, cancellation, and shutdown races are part of the design.

### Separate acquisition from processing

The I/O layer should remain responsive even when application processing is slow.

### Measure before optimizing

Performance decisions should be supported by benchmarks and measurements.

### Prefer simple abstractions

Abstractions should exist because they represent a meaningful responsibility, not merely to increase the number of classes.

---

## Architecture Decision Records

Important architectural choices will be documented in `docs/decisions/`.

Examples include:

* Why IOCP is used
* Why `ReadDirectoryChangesW` is wrapped behind a filesystem abstraction
* Why I/O processing is separated from application processing
* Why event queues are bounded
* How notification overflow is handled
* Why reconciliation is required
* How shutdown is implemented
* When persistence is appropriate
* Why certain synchronization mechanisms were selected

The purpose of these documents is to capture **why** the system works the way it does, not just what the code does.

---

## Current Development Status

> **Early architecture / implementation phase**

The project is being developed incrementally.

### Foundation

* [ ] CMake project
* [ ] Windows build configuration
* [ ] RAII Win32 handle wrapper
* [ ] Win32 error abstraction

### IOCP

* [ ] IOCP context
* [ ] IOCP worker
* [ ] asynchronous operation abstraction
* [ ] completion dispatch
* [ ] cancellation
* [ ] deterministic shutdown

### Filesystem monitoring

* [ ] Directory handle abstraction
* [ ] `ReadDirectoryChangesW`
* [ ] asynchronous notification buffer
* [ ] notification parsing
* [ ] event normalization
* [ ] continuous re-arming
* [ ] multiple directory watches

### Event pipeline

* [ ] Event queue
* [ ] bounded queue
* [ ] backpressure policy
* [ ] event filtering
* [ ] event coalescing
* [ ] rename correlation
* [ ] file stabilization

### Reliability

* [ ] buffer overflow detection
* [ ] reconciliation
* [ ] retry handling
* [ ] directory disappearance recovery
* [ ] shutdown race testing

### Observability

* [ ] runtime statistics
* [ ] queue metrics
* [ ] processing latency
* [ ] error counters
* [ ] logging

### Persistence

* [ ] persistence interface
* [ ] SQLite implementation
* [ ] crash recovery
* [ ] durable queue semantics

### Testing

* [ ] unit tests
* [ ] integration tests
* [ ] stress tests
* [ ] failure injection
* [ ] benchmarks

---

## Roadmap

### Phase 1 — IOCP Foundation

Build and test the reusable IOCP layer.

```text
Win32Handle
    ↓
IocpContext
    ↓
IocpOperation
    ↓
IocpWorker
```

### Phase 2 — Single Directory Watch

Integrate:

```text
ReadDirectoryChangesW
        +
IOCP
```

and implement reliable asynchronous monitoring of one directory.

### Phase 3 — Multiple Watches

Introduce `WatchManager` and support many directories using shared IOCP infrastructure.

### Phase 4 — Event Pipeline

Add:

* normalized events
* bounded queues
* processing workers
* filtering
* coalescing
* rename correlation

### Phase 5 — Reliability

Implement:

* overflow detection
* reconciliation
* retries
* stabilization
* robust cancellation
* deterministic shutdown

### Phase 6 — Persistence

Add optional durable processing infrastructure.

### Phase 7 — Performance and Hardening

Add:

* stress tests
* benchmarks
* failure injection
* performance analysis
* documentation
* production hardening

---

## What This Project Demonstrates

This project is intended to demonstrate practical experience with:

* Modern C++20
* Windows systems programming
* Win32 APIs
* asynchronous I/O
* I/O Completion Ports
* `ReadDirectoryChangesW`
* concurrent programming
* thread lifecycle management
* RAII
* resource ownership
* lock-free / low-lock design considerations
* bounded queues
* backpressure
* event-driven architecture
* failure recovery
* filesystem consistency
* observability
* performance engineering
* automated testing
* CMake
* API design
* architecture documentation

More importantly, it demonstrates the ability to reason about **system behavior under failure and load**, rather than simply use individual APIs.

---

## Non-Goals

The project is intentionally not trying to become:

* a general-purpose Windows file synchronization product
* a backup system
* a cloud storage client
* a GUI filesystem monitor
* a replacement for enterprise endpoint monitoring software
* a general distributed event-processing platform

The primary focus is:

> **Reliable, scalable, asynchronous Windows filesystem event monitoring.**

---

## License

This project is licensed under the MIT License.

See [LICENSE](LICENSE) for details.

---

## Author

**Roman Makhov**

Senior/Principal-level software engineering portfolio project focused on modern C++, Windows systems programming, concurrency, reliability, and production-oriented architecture.
