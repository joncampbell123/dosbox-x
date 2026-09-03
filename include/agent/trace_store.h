#ifndef DOSBOX_AGENT_TRACE_STORE_H
#define DOSBOX_AGENT_TRACE_STORE_H

#include "agent/debugger_adapter.h"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <string>
#include <vector>

namespace dosbox_agent {

struct TraceRegisterChange {
    std::string name;
    std::string value;
};

struct TraceEvent {
    std::uint64_t sequence = 0;
    TraceSample sample;
    std::vector<TraceRegisterChange> register_changes;
};

struct TracePage {
    std::vector<TraceEvent> events;
    bool has_next_cursor = false;
    std::uint64_t next_cursor = 0;
};

class TraceStore {
public:
    explicit TraceStore(std::size_t capacity = 0);

    void Begin(const std::string& detail);
    void Stop();
    void Clear();
    bool IsActive() const;
    const std::string& Detail() const;
    std::size_t EventCount() const;
    void Merge(const std::vector<TraceSample>& samples);
    bool Read(bool has_cursor,
              std::uint64_t cursor,
              std::size_t limit,
              TracePage* page,
              bool* cursor_expired) const;

private:
    void Append(const TraceSample& sample);

    std::size_t capacity = 0;
    bool active = false;
    std::string detail;
    std::size_t merged_sample_count = 0;
    std::uint64_t next_sequence = 1;
    bool has_previous = false;
    RegisterSnapshot previous;
    std::deque<TraceEvent> events;
};

} // namespace dosbox_agent

#endif
