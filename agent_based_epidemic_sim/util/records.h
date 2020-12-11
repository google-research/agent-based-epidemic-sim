#ifndef AGENT_BASED_EPIDEMIC_SIM_UTIL_RECORDS_H_
#define AGENT_BASED_EPIDEMIC_SIM_UTIL_RECORDS_H_

#include "absl/strings/string_view.h"
#include "riegeli/bytes/fd_reader.h"
#include "riegeli/bytes/fd_writer.h"
#include "riegeli/records/record_reader.h"
#include "riegeli/records/record_writer.h"

namespace abesim {

using RiegeliBytesSource = riegeli::FdReader<>;
using RiegeliBytesSink = riegeli::FdWriter<>;

riegeli::RecordReader<RiegeliBytesSource> MakeRecordReader(
    absl::string_view filename);
riegeli::RecordWriter<RiegeliBytesSink> MakeRecordWriter(
    absl::string_view filename, int parallelism);

}  // namespace abesim

#endif  // AGENT_BASED_EPIDEMIC_SIM_UTIL_RECORDS_H_
