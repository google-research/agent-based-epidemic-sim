"""Data loader class of Abesim generated ExposureResult proto in Riegeli format.

Implemented based on the design here:
https://docs.google.com/document/d/1Ra8HrKQY-1ZigeZd8Pr4IpvOCrA2doGhypahiE6qIjw/edit
"""

import datetime
import io
import riegeli
from agent_based_epidemic_sim.applications.risk_learning import exposures_per_test_result_pb2
from agent_based_epidemic_sim.core import pandemic_pb2


class AbesimExposureDataLoader(object):
  """Data loader of Abesim generated ExposureResult proto in Riegeli format."""

  def __init__(self,
               file_path,
               unconfirmed_exposures=False,
               window_around_infection_onset_time=False,
               selection_window_left=10,
               selection_window_right=0,
               file_io=io.FileIO):
    """Initialize the Abesim exposure data loder.

    Args:
      file_path: The path of the Riegeli file that stores ExposureResult protos.
      unconfirmed_exposures: Whether to query unconfirmed exposures.
      window_around_infection_onset_time: Whether to set the exposure selection
        center around the infection onset time (True) or the test administered
        time (False).
      selection_window_left: Days from the left selection bound to the center.
      selection_window_right: Days from the right selection bound to the center.
      file_io: A method for constructing a file object for reading.
    """
    self.file_path = file_path
    self.unconfirmed_exposures = unconfirmed_exposures
    self.window_around_infection_onset_time = window_around_infection_onset_time
    self.selection_window_left = selection_window_left
    self.selection_window_right = selection_window_right
    self.file_io = file_io
    self.index_reader = riegeli.RecordReader(self.file_io(file_path, mode='rb'))

  def __del__(self):
    self.index_reader.close()

  def reset_file(self):
    # Reset to the start of the riegeli file.
    self.index_reader.close()
    self.index_reader = riegeli.RecordReader(
        self.file_io(self.file_path, mode='rb'))

  def _seconds_to_mins(self, timedelta):
    return timedelta.seconds // 60

  def print_all_data(self):
    # Util function to print out all data in the riegeli file.
    for record in self.index_reader.read_messages(
        exposures_per_test_result_pb2.ExposuresPerTestResult.ExposureResult):
      print(record)
    self.reset_file()

  def get_next_batch(self, batch_size=128):
    """Function to query next batch from the index reader."""

    def is_confirmed(exposure):
      return (exposure.exposure_type ==
              exposures_per_test_result_pb2.ExposuresPerTestResult.CONFIRMED)

    def is_valid_unconfirmed(exposure):
      return (exposure.exposure_type == exposures_per_test_result_pb2
              .ExposuresPerTestResult.UNCONFIRMED and
              self.unconfirmed_exposures)

    def is_contained_in_window(exposure, date_window_left, date_window_right):
      return (exposure.exposure_time.ToDatetime() >= date_window_left) and (
          exposure.exposure_time.ToDatetime() <= date_window_right)

    batch_exposure_list = []
    batch_label_list = []
    grouping_list = []
    while (len(batch_label_list) < batch_size and
           self.index_reader.pos.numeric != self.index_reader.size()):
      record = self.index_reader.read_message(
          exposures_per_test_result_pb2.ExposuresPerTestResult.ExposureResult)
      if not record:
        continue
      if not self.window_around_infection_onset_time:
        # Set window around test_administered_date when
        # window_around_infection_onset_time is False.
        test_administered_date = record.test_administered_time.ToDatetime()
        date_window_left = test_administered_date + datetime.timedelta(
            days=-self.selection_window_left)
        date_window_right = test_administered_date + datetime.timedelta(
            days=self.selection_window_right)
      elif record.infection_onset_time:
        # Set window around infection_onset_time when
        # window_around_infection_onset_time is True and
        # record.infection_onset_time is not None.
        infection_date = record.infection_onset_time.ToDatetime()
        date_window_left = infection_date + datetime.timedelta(
            days=-self.selection_window_left)
        date_window_right = infection_date + datetime.timedelta(
            days=self.selection_window_right)
      else:
        # Remove this record if window_around_infection_onset_time is True and
        # record.infection_onset_time is None.
        continue

      exposure_count = 0
      for exposure in record.exposures:
        if (is_confirmed(exposure) or
            is_valid_unconfirmed(exposure)) and (is_contained_in_window(
                exposure, date_window_left, date_window_right)):
          # check if duration_since_symptom_onset is valid or None.
          if exposure.duration_since_symptom_onset:
            duration_since_symptom_onset_day = (
                exposure.duration_since_symptom_onset.ToTimedelta().days)
          else:
            duration_since_symptom_onset_day = None
          # If proximity trace is not present (for example, if
          # TripleExposureGenerator is used to sample exposures), we add one
          # entry to the proximity_trace returned in batch_exposure_list.
          if not exposure.proximity_trace:
            if not exposure.distance:
              raise ValueError('Proximity trace or distance must be present. '
                               'Encountered: %s' % exposure)
            proximity_trace = [exposure.distance]
            proximity_trace_temporal_resolution_minute = self._seconds_to_mins(
                exposure.duration.ToTimedelta())
          else:
            proximity_trace = list(exposure.proximity_trace)
            proximity_trace_temporal_resolution_minute = self._seconds_to_mins(
                exposure.proximity_trace_temporal_resolution.ToTimedelta())
          batch_exposure_list.append(
              (proximity_trace, duration_since_symptom_onset_day,
               proximity_trace_temporal_resolution_minute))

          exposure_count += 1
      if record.outcome == pandemic_pb2.TestOutcome.Outcome.POSITIVE:
        label = 1
      elif record.outcome == pandemic_pb2.TestOutcome.Outcome.NEGATIVE:
        label = 0
      else:
        raise ValueError('Invalid label: %s' % record.outcome)
      batch_label_list.append(label)
      grouping_list.append(exposure_count)

    return batch_exposure_list, batch_label_list, grouping_list
