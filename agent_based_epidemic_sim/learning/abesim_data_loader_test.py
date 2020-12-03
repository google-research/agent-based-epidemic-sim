"""Tests for abesim_data_loader."""

import os

from absl import flags
from absl.testing import absltest

from agent_based_epidemic_sim.learning import abesim_data_loader


FLAGS = flags.FLAGS


class AbesimDataLoaderTest(absltest.TestCase):

  def test_abesim_data_loader(self):
    data_path = os.path.join(
        FLAGS.test_srcdir,
        'agent_based_epidemic_sim/learning/testdata/fake_data_size_30'
    )

    # Test when unconfirmed_exposures is True.
    data_loader = abesim_data_loader.AbesimExposureDataLoader(
        data_path, unconfirmed_exposures=True)

    _, labels, grouping = data_loader.get_next_batch(batch_size=5)
    expect_labels = [1, 2, 0, 1, 2]
    expect_grouping = [0, 0, 0, 0, 0]
    for i in range(5):
      self.assertEqual(labels[i], expect_labels[i])
      self.assertEqual(grouping[i], expect_grouping[i])

    # Test when unconfirmed_exposures is False.
    data_loader = abesim_data_loader.AbesimExposureDataLoader(
        data_path, unconfirmed_exposures=False)
    _, labels, grouping = data_loader.get_next_batch(batch_size=5)
    expect_labels = [1, 2, 0, 1, 2]
    expect_grouping = [0, 0, 0, 0, 0]
    for i in range(5):
      self.assertEqual(labels[i], expect_labels[i])
      self.assertEqual(grouping[i], expect_grouping[i])

    # Test when the required batch size larger than data size in the file.
    data_loader = abesim_data_loader.AbesimExposureDataLoader(
        data_path, unconfirmed_exposures=False)
    _, labels, grouping = data_loader.get_next_batch(batch_size=16)
    self.assertLen(labels, 16)
    self.assertLen(grouping, 16)
    _, labels, grouping = data_loader.get_next_batch(batch_size=16)
    self.assertLen(labels, 14)
    self.assertLen(grouping, 14)


if __name__ == '__main__':
  absltest.main()
