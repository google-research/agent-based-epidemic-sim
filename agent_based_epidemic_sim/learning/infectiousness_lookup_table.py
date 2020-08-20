"""Computes infectiousness level for t days after infection, for t=0:14.

Computes the array of infectiousness levels and prints to the screen.
The results can be stored in  kInfectivityArray in core/constants.h.
"""

from absl import app
from absl import flags
import numpy as np
import scipy.stats

FLAGS = flags.FLAGS


def infectiousness(t):
  """Computes infectiousness level at a given time point.

  Parameters are taken from here:
  https://github.com/BDI-pathogens/OpenABM-Covid19/blob/master/documentation/parameters/infection_parameters.md

  Args:
    t: number of days since infection started.

  Returns:
    Scalar infectiousness level.
  """
  mu = 5.5
  sigma = 2.14
  # For a Gamma(shape=a,rate=inv-scale=b) distribution, mean=a/b, sd=sqrt(a)/b
  # hence b=mu/sigma^2, a=b*mu
  b = mu/(sigma**2)
  a = b*mu
  rv = scipy.stats.gamma(a, loc=0, scale=1/b)
  infect = rv.cdf(t) - rv.cdf(t-1)
  return infect


def main(argv):
  del argv  # Unused
  ts = np.arange(0, 14, 1)
  ps = []
  for t in ts:
    ps.append(infectiousness(t))
  print(ps)

if __name__ == '__main__':
  app.run(main)
