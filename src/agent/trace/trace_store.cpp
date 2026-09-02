#include "agent/trace_store.h"

#include <iomanip>
#include <sstream>

namespace dosbox_agent {
namespace {

std::string Hex32(const std::uint32_t value)
{
    std::ostringstream encoded;
    encoded << "0x" << std::uppercase << std::hex << std::setw(8) << std::setfill('0') << value;
    return encoded.str();
}

std::string Hex16(const std::uint16_t value)
{
    std::ostringstream encoded;
    encoded << "0x" << std::uppercase << std::hex << std::setw(4) << std::setfill('0') << value;
    return encoded.str();
}

void AddChange(std::vector<TraceRegisterChange>* changes,
               const char* name,
               const std::string& value,
               const bool changed)
{
    if (!changed)
        return;
    TraceRegisterChange change;
    change.name = name;
    change.value = value;
    changes->push_back(change);
}

} // namespace

TraceStore::TraceStore(const std::size_t capacity) : capacity(capacity)
{}

void TraceStore::Begin(const std::string& requested_detail)
{
    Clear();
    active = true;
    detail = requested_detail;
}

void TraceStore::Stop()
{
    active = false;
}

void TraceStore::Clear()
{
    active = false;
    detail.clear();
    merged_sample_count = 0;
    next_sequence = 1;
    has_previous = false;
    previous = RegisterSnapshot();
    events.clear();
}

bool TraceStore::IsActive() const
{
    return active;
}

const std::string& TraceStore::Detail() const
{
    return detail;
}

std::size_t TraceStore::EventCount() const
{
    return events.size();
}

void TraceStore::Merge(const std::vector<TraceSample>& samples)
{
    if (samples.size() < merged_sample_count)
        return;
    for (std::size_t index = merged_sample_count; index < samples.size(); ++index)
        Append(samples[index]);
    merged_sample_count = samples.size();
}

void TraceStore::Append(const TraceSample& sample)
{
    TraceEvent event;
    event.sequence = next_sequence++;
    event.sample = sample;
    const RegisterSnapshot& current = sample.registers;
    const RegisterSnapshot& prior = previous;
    const bool first = !has_previous;

    AddChange(&event.register_changes, "eax", Hex32(current.eax), first || current.eax != prior.eax);
    AddChange(&event.register_changes, "ebx", Hex32(current.ebx), first || current.ebx != prior.ebx);
    AddChange(&event.register_changes, "ecx", Hex32(current.ecx), first || current.ecx != prior.ecx);
    AddChange(&event.register_changes, "edx", Hex32(current.edx), first || current.edx != prior.edx);
    AddChange(&event.register_changes, "esi", Hex32(current.esi), first || current.esi != prior.esi);
    AddChange(&event.register_changes, "edi", Hex32(current.edi), first || current.edi != prior.edi);
    AddChange(&event.register_changes, "ebp", Hex32(current.ebp), first || current.ebp != prior.ebp);
    AddChange(&event.register_changes, "esp", Hex32(current.esp), first || current.esp != prior.esp);
    AddChange(&event.register_changes, "cs", Hex16(current.cs), first || current.cs != prior.cs);
    AddChange(&event.register_changes, "ds", Hex16(current.ds), first || current.ds != prior.ds);
    AddChange(&event.register_changes, "es", Hex16(current.es), first || current.es != prior.es);
    AddChange(&event.register_changes, "fs", Hex16(current.fs), first || current.fs != prior.fs);
    AddChange(&event.register_changes, "gs", Hex16(current.gs), first || current.gs != prior.gs);
    AddChange(&event.register_changes, "ss", Hex16(current.ss), first || current.ss != prior.ss);
    AddChange(&event.register_changes, "instruction_pointer", Hex32(current.instruction_pointer),
              first || current.instruction_pointer != prior.instruction_pointer);
    AddChange(&event.register_changes, "flags", Hex32(current.flags), first || current.flags != prior.flags);

    previous = current;
    has_previous = true;
    events.push_back(event);
    while (capacity != 0 && events.size() > capacity)
        events.pop_front();
}

bool TraceStore::Read(const bool has_cursor,
                      const std::uint64_t cursor,
                      const std::size_t limit,
                      TracePage* page,
                      bool* cursor_expired) const
{
    if (page == NULL || cursor_expired == NULL || limit == 0)
        return false;
    page->events.clear();
    page->has_next_cursor = false;
    page->next_cursor = 0;
    *cursor_expired = false;

    if (events.empty()) {
        if (has_cursor && cursor != 0) {
            *cursor_expired = true;
            return false;
        }
        return true;
    }

    const std::uint64_t first_sequence = events.front().sequence;
    const std::uint64_t last_sequence = events.back().sequence;
    if (has_cursor && (cursor + 1 < first_sequence || cursor > last_sequence)) {
        *cursor_expired = true;
        return false;
    }

    const std::uint64_t first_requested = has_cursor ? cursor + 1 : first_sequence;
    for (std::deque<TraceEvent>::const_iterator it = events.begin(); it != events.end(); ++it) {
        if (it->sequence < first_requested)
            continue;
        if (page->events.size() == limit)
            break;
        page->events.push_back(*it);
    }
    if (!page->events.empty() && page->events.back().sequence < last_sequence) {
        page->has_next_cursor = true;
        page->next_cursor = page->events.back().sequence;
    }
    return true;
}

} // namespace dosbox_agent
