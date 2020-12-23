#include "agent_based_epidemic_sim/core/exposure_store.h"

#include "agent_based_epidemic_sim/core/event.h"

namespace abesim {

ExposureStore::ExposureStore() : buffer_(14) {}

void ExposureStore::GarbageCollect(absl::Time before) {
  while (head_ != tail_ && buffer_[head_].exposure.start_time < before) {
    Record& record = buffer_[head_];
    record.contact_report.reset();
    if (record.newer_id == 0) {  // This was the agents only record.
      agents_.erase(record.uuid);
    } else {
      Sentinel& sentinel = agents_[record.uuid];
      sentinel.oldest_id = record.newer_id;
      GetRecordById(sentinel.oldest_id).older_id = 0;
    }
    head_ = (head_ + 1) % buffer_.size();
    head_id_++;
  }
}

void ExposureStore::AddExposures(
    absl::Span<const InfectionOutcome> infection_outcomes) {
  // Ensure there is enough space for all the new records.
  const size_t current = size();
  const size_t desired = infection_outcomes.size() + current;
  if (desired >= buffer_.size()) {
    // Reallocate the circular buffer.
    std::vector<Record> tmp(std::max(buffer_.size() * 2, desired + 1));
    size_t id = head_id_;
    for (int i = 0; i < current; ++i) {
      tmp[i] = std::move(GetRecordById(id++));
    }
    std::swap(buffer_, tmp);
    head_ = 0;
    tail_ = current;
  }

  // Add new records to the tail of the buffer.
  tail_ = (tail_ + infection_outcomes.size()) % buffer_.size();
  size_t next_id = head_id_ + current;
  for (const InfectionOutcome& outcome : infection_outcomes) {
    Record& record = GetRecordById(next_id);
    record.uuid = outcome.source_uuid;
    record.exposure = outcome.exposure;

    Sentinel& sentinel = agents_[record.uuid];
    record.older_id = sentinel.newest_id;
    record.newer_id = 0;
    if (sentinel.newest_id != 0) {
      GetRecordById(sentinel.newest_id).newer_id = next_id;
    } else {
      sentinel.oldest_id = next_id;
    }
    sentinel.newest_id = next_id;
    next_id++;
  }
}

const ExposureStore::Record& ExposureStore::GetRecordById(
    const size_t i) const {
  DCHECK_GE(i, head_id_);
  DCHECK_LT(i - head_id_, size());
  const size_t idx = (head_ + i - head_id_) % buffer_.size();
  return buffer_[idx];
}

ExposureStore::Record& ExposureStore::GetRecordById(const size_t i) {
  DCHECK_GE(i, head_id_);
  DCHECK_LT(i - head_id_, size());
  const size_t idx = (head_ + i - head_id_) % buffer_.size();
  return buffer_[idx];
}

size_t ExposureStore::size() const {
  if (tail_ >= head_) return tail_ - head_;
  return buffer_.size() - head_ + tail_;
}

}  // namespace abesim
