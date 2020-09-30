"""Tests for ens_simulator."""
from absl.testing import absltest
import numpy as np
from agent_based_epidemic_sim.learning import ens_simulator


class EnsSimulatorTest(absltest.TestCase):

  def setUp(self):
    super().setUp()
    self.atten = np.array([50])
    self.duration = np.array([15])
    self.symptom_days = np.array([-5])
    self.params = ens_simulator.ModelParams()

  def test_score_increases_with_increased_duration(self):

    interaction_long = np.array([30])
    interaction_short = np.array([15])

    prob_long = ens_simulator.prob_infection_batch(
        attenuations=self.atten,
        durations=interaction_long,
        symptom_days=self.symptom_days,
        params=self.params)
    prob_short = ens_simulator.prob_infection_batch(
        attenuations=self.atten,
        durations=interaction_short,
        symptom_days=self.symptom_days,
        params=self.params)

    self.assertGreater(prob_long, prob_short)

  def test_score_increases_with_decreased_attenuation(self):

    attenuation_far = np.array([80])
    attenuation_near = np.array([30])

    prob_far = ens_simulator.prob_infection_batch(
        attenuations=attenuation_far,
        durations=self.duration,
        symptom_days=self.symptom_days,
        params=self.params)
    prob_near = ens_simulator.prob_infection_batch(
        attenuations=attenuation_near,
        durations=self.duration,
        symptom_days=self.symptom_days,
        params=self.params)

    self.assertGreater(prob_near, prob_far)


if __name__ == '__main__':
  absltest.main()
