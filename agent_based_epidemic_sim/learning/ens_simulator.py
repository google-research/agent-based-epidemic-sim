"""COVID-19 Exposure Notification System Risk Simulator.

This library provides the required methods to compute the probability of
COVID-19 transmission from a host to a susceptible individual, as a function of
distance (estimated from bluetooth attenuation), duration, and infectiousness of
the host (estimated based on days since symtom onset). This is done using a
standard exponential dose response model, such that,

h(x) = t x f_dist(d) x f_inf(delta)

where, h is the hazard function, x is an interaction between a host and a
susceptible, t is the duration of the interaction in minutes, f_dist(d) is the
rate of viral particles at meters of distance d per unit time, and f_inf(delta)
is the conditional probability of infectious transmission for days since
symptom-onset delta. The parameters of this model are derived from recent papers
on the epidemiology of COVID, and on empirical bluetooth attenuation data.

For a Colab demonstrating this code, go to (broken link).
"""

from dataclasses import dataclass, field
import numpy as np
import scipy.stats


def infectiousness_gaussian(deltas: np.ndarray) -> np.ndarray:
  """Infectiousness as a Gaussian distribution (Briers et al., 2020).

  Args:
    deltas: np.array of days since symptom onset.

  Returns:
    Relative probability of infectious transmission.
  """
  mu = -0.3
  s = 2.75
  ps = np.exp(-np.power(deltas - mu, 2) / (2 * s * s))
  return ps


def skew_logistic_scaled(x: np.ndarray,
                         alpha: float,
                         mu: float,
                         sigma: float) -> np.ndarray:
  """Calculates the density of the skew logistic PDF (Feretti et al., 2020).

  Args:
    x: np.array of days since symptom onset.
    alpha: The shape parameter of the skew logistic as a float.
    mu: The location parameter of the skew logistic as a float.
    sigma: The scale parameter of the skew logistic as a float.

  Returns:
    Relative probability of infectious transmission.
  """
  return scipy.stats.genlogistic.pdf(x, alpha, loc=mu, scale=sigma)


def ptost_conditional(deltas: np.ndarray,
                      incubation: np.ndarray) -> np.ndarray:
  """Probability of transmission conditional on incubation.

  ptost(t|k) / max_t(ptost(t|k)),
  where, t = days since symptom onset (delta) and k = days of incubation.

  Args:
    deltas: np.array of days since symptom onset.
    incubation: np.array of days of incubation (time from infection to
      symptoms).

  Returns:
    Probability of infectious transmission.
  """
  mu = -4
  sigma = 1.85
  alpha = 5.85
  tau = 5.42
  fpos = skew_logistic_scaled(deltas, alpha, mu, sigma)
  # Error in paper
  # fneg = skew_logistic_scaled(ts, alpha, mu, sigma*incubation/tau)
  fneg = skew_logistic_scaled(deltas * tau / incubation, alpha, mu, sigma)
  ps = fpos
  neg = np.where(deltas < 0)
  ps[neg] = fneg[neg]
  ps = ps / np.max(ps)
  return ps


def incubation_dist(t: np.ndarray) -> np.ndarray:
  """Incubation period as a log-normal PDF (Lauer et al., 2020).

  Args:
    t: np.array of days of incubation (i.e. days from infection to symptoms).

  Returns:
    Relative probability of the specified incubation time.
  """
  mu = 1.621
  sig = 0.418
  rv = scipy.stats.lognorm(sig, scale=np.exp(mu))
  return rv.pdf(t)


def ptost_uncond(deltas: np.ndarray) -> np.ndarray:
  """Marginal probability of transmission (Lauer et al., 2020).

  p(t) = sum_{k=1}^14 p(incubation=k) ptost(t|k) / max_t(ptost(t|k)),
  where, t = days since symptom onset (delta) and k = days of incubation.

  Args:
    deltas: np.array of days since symptom onset (TOST).

  Returns:
    Probability of infectious transmission.
  """
  incub_times = np.arange(1, 14, 1)
  incub_probs = incubation_dist(incub_times)
  tost_probs = np.zeros_like(deltas, dtype=float)
  for k, incub in enumerate(incub_times):
    ps = ptost_conditional(deltas, incub)
    tost_probs += incub_probs[k] * ps
  return tost_probs


def infectiousness_skew_logistic(deltas: np.ndarray) -> np.ndarray:
  """Infectiousness as a skew-logistic distribution (Ferretti et al., 2020).

  Args:
    deltas: np.array of days since symptom onset.

  Returns:
    Probability of infectious transmission.
  """
  infectiousness_times = np.arange(-14, 14 + 1, 0.1)
  infectiousness_vals = ptost_uncond(infectiousness_times)
  return np.interp(deltas, infectiousness_times, infectiousness_vals)


def dose_curve_quadratic(distances: np.ndarray, dmin: float = 1) -> np.ndarray:
  """Dose as a quadratic function of distance (Briers et al., 2020).

  Args:
    distances: np.array of meters of distance from the infector.
    dmin: 1 based on physics of droplet spread.

  Returns:
    Dose rate in arbitrary units per unit time.
  """
  m = np.power(dmin, 2) / np.power(distances, 2)
  return np.minimum(1, m)


def dose_curve_sigmoid(distances: np.ndarray,
                       slope: float = 1.5,
                       inflection: float = 4.4) -> np.ndarray:
  """Dose as a sigmoidal decay function of distance (Cencetti et al., 2020).

  Args:
    distances: np.array of meters of distance from the infector.
    slope: the slope of the model heuristically defined as 1.5.
    inflection: the inflection point of the model heuristically defined as 4.4.

  Returns:
    Dose rate in arbitrary units per unit time.
  """
  return 1 - 1 / (1 + np.exp(-slope * distances + slope * inflection))


@dataclass
class BleParams:
  """Defines parameters required in converting bluetooth attenuation signals captured by a bluetooth low energy (BLE) sensor into units of distance.
  """
  slope: float = 0.21
  intercept: float = 3.92
  sigma: float = np.sqrt(0.33)
  tx: float = 0.0
  correction: float = 2.398
  name: str = 'briers-lognormal'


def atten_to_dist(attens: np.ndarray, params: BleParams) -> np.ndarray:
  """Converts bluetooth attenuation into expected distance (Lovett et al., 2020).

  Args:
    attens: np.array of attenuation of the bluetooth signal in dB (decibels).
    params: A BleParams object.

  Returns:
    Expected distance in meters as an np.array.
  """
  rssi = params.tx - (attens + params.correction)
  return np.exp((np.log(-rssi) - params.intercept) / params.slope)


def dist_to_atten(distances: np.ndarray, params: BleParams) -> np.ndarray:
  """Converts distance into expected bluetooth attenuation (Lovett et al., 2020).

  Args:
    distances: np.array of distances in meters.
    params: A BleParams object.

  Returns:
    Expected attenuation in dB (decibels) as an np.array.
  """
  mu = params.intercept + params.slope * np.log(distances)
  rssi = -np.exp(mu)
  attens = params.tx - (rssi + params.correction)
  return attens


def dist_to_atten_sample(distances: np.ndarray,
                         params: BleParams) -> np.ndarray:
  """Samples from the lognormal distribution of attenuations for each distance.

  Args:
    distances: np.array of distances in meters.
    params: A BleParams object.

  Returns:
    Attenuation samples in dB as an np.array.
  """
  if params.sigma == 0:
    return dist_to_atten(distances, params)
  mus = params.intercept + params.slope * np.log(distances)
  sigs = params.sigma
  rssi = -scipy.stats.lognorm(s=sigs, scale=np.exp(mus)).rvs()
  attens = params.tx - (rssi + params.correction)
  return attens


@dataclass
class ModelParams:
  """Defines all parameters required to define the distributions of distance, dose and infectiousness.
  """
  ble_params: BleParams = field(
      default_factory=BleParams
  )  # may want to change sigma
  distance_fun: str = 'sigmoid'  # quadratic or sigmoid
  distance_dmin: float = 1.0
  distance_slope: float = 1.5
  distance_inflection: float = 4.4
  infectiousness_fun: str = 'skew-logistic'  # gaussian or skew-logistic
  beta: float = 1e-3  # transmission rate


def prob_infection_batch(attenuations: np.ndarray,
                         durations: np.ndarray,
                         symptom_days: np.ndarray,
                         params: ModelParams,
                         distances: np.ndarray = None) -> np.ndarray:
  """Calculates the probability of infection given observed values and distributional assumptions.

  Args:
    attenuations: np.array of bluetooth signal attenuation in dB (decibels).
    durations: np.array of interaction duration in minutes.
    symptom_days: np.array of days since symptom onset.
    params: a ModelParams object.
    distances: np.array of distance in meters.

  Returns:
    Probability of infectious transmision.
  """
  if distances is None:
    distances = atten_to_dist(attenuations, params.ble_params)
  if params.distance_fun == 'quadratic':
    fd = dose_curve_quadratic(distances, params.distance_dmin)
  elif params.distance_fun == 'sigmoid':
    fd = dose_curve_sigmoid(distances, params.distance_slope,
                            params.distance_inflection)
  if params.infectiousness_fun == 'gaussian':
    finf = infectiousness_gaussian(symptom_days)
  elif params.infectiousness_fun == 'skew-logistic':
    finf = infectiousness_skew_logistic(symptom_days)
  doses = durations * fd * finf
  return 1 - np.exp(-params.beta * doses)
