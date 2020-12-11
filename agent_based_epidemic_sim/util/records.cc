#include "agent_based_epidemic_sim/util/records.h"

namespace abesim {

constexpr int kReadFlag = O_RDONLY;
constexpr int kWriteFlag = O_CREAT | O_WRONLY;

riegeli::RecordReader<RiegeliBytesSource> MakeRecordReader(
    absl::string_view filename) {
  return riegeli::RecordReader<RiegeliBytesSource>(
      std::forward_as_tuple(filename, kReadFlag));
}
riegeli::RecordWriter<RiegeliBytesSink> MakeRecordWriter(
    absl::string_view filename, const int parallelism) {
  return riegeli::RecordWriter<RiegeliBytesSink>(
      std::forward_as_tuple(filename, kWriteFlag),
      riegeli::RecordWriterBase::Options().set_parallelism(parallelism));
}

}  // namespace abesim
