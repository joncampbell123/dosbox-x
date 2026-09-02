#include "agent/agent_bridge.h"

#include <atomic>
#include <chrono>
#include <deque>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

namespace dosbox_agent {

class EmulationThreadQueue::Impl {
public:
    struct PendingCommand {
        std::uint64_t sequence = 0;
        std::chrono::steady_clock::time_point not_before;
        Command command;
    };

    mutable std::mutex mutex;
    std::deque<PendingCommand> commands;
    std::thread::id emulation_thread;
    std::uint64_t next_sequence = 1;
    bool attached = false;
    bool shutdown = false;
};

EmulationThreadQueue::EmulationThreadQueue() : impl(new Impl())
{}

EmulationThreadQueue::~EmulationThreadQueue()
{
    delete impl;
}

bool EmulationThreadQueue::BindToCurrentThread()
{
    std::lock_guard<std::mutex> lock(impl->mutex);
    const std::thread::id current = std::this_thread::get_id();
    if (impl->shutdown)
        return false;
    if (impl->attached)
        return impl->emulation_thread == current;

    impl->attached = true;
    impl->emulation_thread = current;
    return true;
}

bool EmulationThreadQueue::IsBoundToCurrentThread() const
{
    std::lock_guard<std::mutex> lock(impl->mutex);
    return impl->attached && impl->emulation_thread == std::this_thread::get_id();
}

std::uint64_t EmulationThreadQueue::Submit(Command command)
{
    return SubmitAfter(0, std::move(command));
}

std::uint64_t EmulationThreadQueue::SubmitAfter(const std::uint32_t delay_ms, Command command)
{
    if (!command)
        return 0;

    std::lock_guard<std::mutex> lock(impl->mutex);
    if (impl->shutdown || impl->next_sequence == 0)
        return 0;

    const std::uint64_t sequence = impl->next_sequence++;
    Impl::PendingCommand pending;
    pending.sequence = sequence;
    pending.not_before = std::chrono::steady_clock::now() + std::chrono::milliseconds(delay_ms);
    pending.command = std::move(command);
    impl->commands.push_back(std::move(pending));
    return sequence;
}

std::size_t EmulationThreadQueue::Pump()
{
    if (!IsBoundToCurrentThread())
        return 0;

    std::size_t pending_count = 0;
    {
        std::lock_guard<std::mutex> lock(impl->mutex);
        pending_count = impl->commands.size();
    }

    std::size_t count = 0;
    std::size_t inspected = 0;
    while (inspected < pending_count) {
        Impl::PendingCommand pending;
        {
            std::lock_guard<std::mutex> lock(impl->mutex);
            if (impl->commands.empty())
                return count;
            pending = std::move(impl->commands.front());
            impl->commands.pop_front();
            ++inspected;
            if (pending.not_before > std::chrono::steady_clock::now()) {
                impl->commands.push_back(std::move(pending));
                continue;
            }
        }

        pending.command(pending.sequence);
        ++count;
    }
    return count;
}

void EmulationThreadQueue::Shutdown()
{
    std::lock_guard<std::mutex> lock(impl->mutex);
    impl->shutdown = true;
    while (!impl->commands.empty())
        impl->commands.pop_front();
}

EmulationThreadQueue& AGENT_EmulationQueue()
{
    static EmulationThreadQueue queue;
    return queue;
}

namespace {

std::mutex debugger_stop_listener_mutex;
DebuggerStopListener debugger_stop_listener;

} // namespace

void AGENT_BridgeAttachToCurrentThread()
{
    AGENT_EmulationQueue().BindToCurrentThread();
}

std::size_t AGENT_BridgePump()
{
    return AGENT_EmulationQueue().Pump();
}

void AGENT_BridgeShutdown()
{
    AGENT_EmulationQueue().Shutdown();
    AGENT_SetDebuggerStopListener(DebuggerStopListener());
}

void AGENT_SetDebuggerStopListener(DebuggerStopListener listener)
{
    std::lock_guard<std::mutex> lock(debugger_stop_listener_mutex);
    debugger_stop_listener = std::move(listener);
}

void AGENT_NotifyDebuggerStopped(const std::uint16_t segment,
                                 const std::uint32_t instruction_pointer)
{
    DebuggerStopListener listener;
    {
        std::lock_guard<std::mutex> lock(debugger_stop_listener_mutex);
        listener = debugger_stop_listener;
    }
    if (listener)
        listener(segment, instruction_pointer);
}

bool AGENT_RunQueueSelfTest(std::string* error)
{
    if (error == NULL)
        return false;

    error->clear();
    EmulationThreadQueue queue;
    if (!queue.BindToCurrentThread()) {
        *error = "Unable to bind queue to the self-test thread";
        return false;
    }

    std::atomic<int> foreign_calls(0);
    if (queue.Submit([&foreign_calls](const std::uint64_t) {
            ++foreign_calls;
        }) == 0) {
        *error = "Unable to submit foreign-thread guard command";
        return false;
    }

    std::size_t foreign_pump_count = 1;
    std::thread foreign_thread([&queue, &foreign_pump_count]() {
        foreign_pump_count = queue.Pump();
    });
    foreign_thread.join();
    if (foreign_pump_count != 0 || foreign_calls.load() != 0 || queue.Pump() != 1 || foreign_calls.load() != 1) {
        *error = "Queue allowed a non-emulation thread to execute a command";
        return false;
    }

    const std::size_t command_count = 1000;
    std::vector<std::uint64_t> completed;
    completed.reserve(command_count);
    std::atomic<std::size_t> next_command(0);
    std::atomic<std::size_t> rejected(0);
    std::vector<std::thread> workers;
    for (std::size_t worker = 0; worker < 8; ++worker) {
        workers.emplace_back([&queue, &completed, &next_command, &rejected, command_count]() {
            for (;;) {
                const std::size_t command = next_command.fetch_add(1);
                if (command >= command_count)
                    return;
                if (queue.Submit([&completed](const std::uint64_t sequence) {
                        completed.push_back(sequence);
                    }) == 0)
                    ++rejected;
            }
        });
    }
    for (std::vector<std::thread>::iterator worker = workers.begin(); worker != workers.end(); ++worker)
        worker->join();

    if (rejected.load() != 0 || queue.Pump() != command_count || completed.size() != command_count) {
        *error = "Queue did not process every submitted command";
        return false;
    }
    for (std::size_t index = 0; index < completed.size(); ++index) {
        if (completed[index] != index + 2) {
            *error = "Queue did not preserve submission order";
            return false;
        }
    }

    std::atomic<int> delayed_calls(0);
    if (queue.SubmitAfter(20, [&delayed_calls](const std::uint64_t) {
            ++delayed_calls;
        }) == 0 || queue.Pump() != 0 || delayed_calls.load() != 0) {
        *error = "Queue did not defer a delayed command";
        return false;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(25));
    if (queue.Pump() != 1 || delayed_calls.load() != 1) {
        *error = "Queue did not execute a delayed command after its deadline";
        return false;
    }

    if (queue.SubmitAfter(20, [&delayed_calls](const std::uint64_t) {
            ++delayed_calls;
        }) == 0) {
        *error = "Unable to submit delayed shutdown command";
        return false;
    }

    queue.Shutdown();
    std::this_thread::sleep_for(std::chrono::milliseconds(25));
    if (queue.Pump() != 0 || delayed_calls.load() != 1) {
        *error = "Queue executed a delayed command after shutdown";
        return false;
    }
    return true;
}

} // namespace dosbox_agent
