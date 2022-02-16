#include "agent_based_epidemic_sim/util/records.h"

namespace abesim {

riegeli::RecordReader<RiegeliBytesSource> MakeRecordReader(
    absl::string_view filename) {
  return riegeli::RecordReader(RiegeliBytesSource(filename));
}
riegeli::RecordWriter<RiegeliBytesSink> MakeRecordWriter(
    absl::string_view filename, const int parallelism) {
  return riegeli::RecordWriter(
      RiegeliBytesSink(filename),
      riegeli::RecordWriterBase::Options().set_parallelism(parallelism));
}

}  // namespace abesim
