#ifndef AGENT_BASED_EPIDEMIC_SIM_CORE_EXPOSURE_STORE_H_
#define AGENT_BASED_EPIDEMIC_SIM_CORE_EXPOSURE_STORE_H_

#include <functional>
#include <limits>
#include <memory>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/time/time.h"
#include "absl/types/optional.h"
#include "agent_based_epidemic_sim/core/event.h"

namespace abesim {

class ExposureStore {
 public:
  ExposureStore();

  // Delete exposures that started before the given time.
  void GarbageCollect(absl::Time before);

  // Add exposures to the store.  Exposures must be added to the store in
  // chronological order.
  void AddExposures(absl::Span<const InfectionOutcome> infection_outcomes);

  // Call fn for every agent with an exposure that starts on or after the given
  // time. Note that fn will be called at most once for a given uuid. Fn should
  // have the signature void(int64 uuid).
  template <typename Fn>
  void PerAgent(absl::Time since, Fn fn) const;

  // Call fn for every exposure with the given uuid that has not already been
  // processed by a previous ProcessNotification call. Fn should have signature
  // void(const Exposure&).
  template <typename Fn>
  void ProcessNotification(const ContactReport& report, Fn fn);

  // Call fn for every exposure in the store that started on or after since.
  // Fn should have signature:
  // void(int64 uuid, const Exposure&, const ContactReport*).
  // Note that the ContactReport may be nullptr if no exposure notification
  // was recieved for the given exposure and in any case the pointer is only
  // valid until the next call to GarbageCollect.
  template <typename Fn>
  void PerExposure(absl::Time since, Fn fn) const;

  // Return the number of exposures currently stored.
  size_t size() const;

 private:
  struct Record {
    size_t newer_id = 0;
    size_t older_id = 0;
    int64 uuid;
    Exposure exposure;
    std::unique_ptr<ContactReport> contact_report;
  };
  const Record& GetRecordById(size_t i) const;
  Record& GetRecordById(size_t i);

  // We need stable ids for every record we've ever seen.  We start with the id
  // 1 so that 0 is an invalid id.  The id of any element in the circular buffer
  // is just head_id_ + i where i is the index of the record in the
  // buffer.
  size_t head_id_ = 1;

  // agents_ gives quick access to the list of records for each agent.
  struct Sentinel {
    size_t oldest_id = 0;
    size_t newest_id = 0;
  };
  absl::flat_hash_map<int64, Sentinel> agents_;

  // buffer_ is a circular buffer, the oldest exposures index is head_,
  // tail_ is one past the index of the newest exposure.  If head_ == tail_ the
  // buffer is empty.  Note that indexes and ids are not the same.
  size_t head_ = 0;
  size_t tail_ = 0;
  std::vector<Record> buffer_;
};

template <typename Fn>
void ExposureStore::PerAgent(absl::Time since, Fn fn) const {
  thread_local absl::flat_hash_set<int64> visited;
  visited.clear();
  for (size_t id = head_id_ + size() - 1; id >= head_id_; --id) {
    const Record& record = GetRecordById(id);
    if (record.exposure.start_time < since) break;
    auto [iter, inserted] = visited.insert(record.uuid);
    if (inserted) fn(record.uuid);
  }
}

template <typename Fn>
void ExposureStore::ProcessNotification(const ContactReport& report, Fn fn) {
  auto iter = agents_.find(report.from_agent_uuid);
  if (iter == agents_.end()) return;
  size_t id = iter->second.newest_id;
  while (id != 0) {
    Record& record = GetRecordById(id);
    if (record.contact_report != nullptr) break;
    record.contact_report = absl::make_unique<ContactReport>(report);
    fn(record.exposure);
    id = record.older_id;
  }
}

template <typename Fn>
void ExposureStore::PerExposure(absl::Time since, Fn fn) const {
  for (size_t id = head_id_ + size() - 1; id >= head_id_; --id) {
    const Record& record = GetRecordById(id);
    if (record.exposure.start_time < since) break;
    fn(record.uuid, record.exposure, record.contact_report.get());
  }
}

}  // namespace abesim

#endif  // AGENT_BASED_EPIDEMIC_SIM_CORE_EXPOSURE_STORE_H_
