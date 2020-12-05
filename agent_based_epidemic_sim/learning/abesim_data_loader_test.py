"""Tests for abesim_data_loader."""

import io
import os
import tempfile

from absl.testing import absltest
import riegeli

from google.protobuf import text_format
from agent_based_epidemic_sim.applications.risk_learning import exposures_per_test_result_pb2
from agent_based_epidemic_sim.learning import abesim_data_loader

_TMP_FILE_NAME = 'exposures.riegeli'
_TEST_FILE_PATH = ('agent_based_epidemic_sim/agent_based_epidemic_sim/learning/'
                   'testdata/fake_data_size_30')


def _get_exposures_without_proximity_traces():
  return [
      text_format.Parse(
          """
            test_administered_time { seconds: 86400 }
            exposures {
              exposure_time { seconds: 43200 }
              duration_since_symptom_onset { }
              proximity_trace_temporal_resolution { }
              exposure_type: CONFIRMED
              duration { seconds: 1800 }
              distance: 1.0
              attenuation: 1.0
            }
           """,
          exposures_per_test_result_pb2.ExposuresPerTestResult.ExposureResult())
  ]


def _get_exposures_missing_required_data():
  return [
      text_format.Parse(
          """
            test_administered_time { seconds: 86400 }
            exposures {
              exposure_time { seconds: 43200 }
              duration_since_symptom_onset { }
              proximity_trace_temporal_resolution { }
              exposure_type: CONFIRMED
            }
           """,
          exposures_per_test_result_pb2.ExposuresPerTestResult.ExposureResult())
  ]


def _get_test_data_path():
  return os.path.join(absltest.get_default_test_srcdir(), _TEST_FILE_PATH)


def _get_tmp_file_name():
  tmp_dir = tempfile.mkdtemp(dir=absltest.get_default_test_tmpdir())
  return os.path.join(tmp_dir, _TMP_FILE_NAME)


class AbesimDataLoaderTest(absltest.TestCase):

  def test_reads_with_traces_unconfirmed_exposures(self):
    data_loader = abesim_data_loader.AbesimExposureDataLoader(
        _get_test_data_path(), unconfirmed_exposures=True)
    _, labels, grouping = data_loader.get_next_batch(batch_size=5)
    expect_labels = [1, 2, 0, 1, 2]
    expect_grouping = [0, 0, 0, 0, 0]
    self.assertCountEqual(labels, expect_labels)
    self.assertCountEqual(grouping, expect_grouping)

  def test_reads_with_traces_no_unconfirmed_exposures(self):
    data_loader = abesim_data_loader.AbesimExposureDataLoader(
        _get_test_data_path(), unconfirmed_exposures=False)
    _, labels, grouping = data_loader.get_next_batch(batch_size=5)
    expect_labels = [1, 2, 0, 1, 2]
    expect_grouping = [0, 0, 0, 0, 0]
    self.assertCountEqual(labels, expect_labels)
    self.assertCountEqual(grouping, expect_grouping)

  def test_reads_with_traces_batch_size_too_large(self):
    data_loader = abesim_data_loader.AbesimExposureDataLoader(
        _get_test_data_path(), unconfirmed_exposures=False)
    _, labels, grouping = data_loader.get_next_batch(batch_size=16)
    self.assertLen(labels, 16)
    self.assertLen(grouping, 16)
    _, labels, grouping = data_loader.get_next_batch(batch_size=16)
    self.assertLen(labels, 14)
    self.assertLen(grouping, 14)

  def test_reads_without_traces(self):
    filename = _get_tmp_file_name()
    with riegeli.RecordWriter(
        io.FileIO(filename, mode='wb'),
        options='transpose',
        metadata=riegeli.RecordsMetadata()) as writer:
      writer.write_messages(_get_exposures_without_proximity_traces())

    data_loader = abesim_data_loader.AbesimExposureDataLoader(
        filename, unconfirmed_exposures=False)
    exposures, labels, grouping = data_loader.get_next_batch(batch_size=1)
    self.assertCountEqual(exposures, [([1.0], 0, 30)])
    self.assertCountEqual(labels, [0])
    self.assertCountEqual(grouping, [1])

  def test_reads_without_traces_or_triple_raises(self):
    filename = _get_tmp_file_name()
    with riegeli.RecordWriter(
        io.FileIO(filename, mode='wb'),
        options='transpose',
        metadata=riegeli.RecordsMetadata()) as writer:
      writer.write_messages(_get_exposures_missing_required_data())

    data_loader = abesim_data_loader.AbesimExposureDataLoader(
        filename, unconfirmed_exposures=False)
    with self.assertRaises(ValueError):
      _ = data_loader.get_next_batch(batch_size=1)


if __name__ == '__main__':
  absltest.main()
