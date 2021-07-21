# Agent Based Epidemic Simulator

[![Build Status](https://travis-ci.org/google-research/agent-based-epidemic-sim.svg?branch=develop)](https://travis-ci.org/google-research/agent-based-epidemic-sim/branches)

This repository hosts an open source agent-based simulator for modeling
epidemics at large scale (nation-scale) using parallel and distributed
computation.

At a high level, the simulator enables users to model the movement of simulated
agents between different locations, and computes the spread of disease based on
the interactions between agents who are colocated. The simulator also has
interfaces to model the impact of social distancing policies, contact tracing
mechanisms, and testing regimens on disease spread.

All aspects of the simulation are parameterized. The user of the simulation is
able to define distributions which dictate simulation behavior such as:

*   The size of the population
*   The size and number of locations to visit
*   The length of visits to the locations
*   The between-host transmissibility of the disease
*   The state transition diagram and dwell times of the disease

These parameters can be configured to apply to particular scenarios and can be
calibrated against real world data for use in experiments. Please note that this
simulator is meant to explore the relative impact and sensitivity of various
conditions and interventions and its results must not be interpreted as
definitive about exact real world outcomes.

Specific applications of the simulator can be found in the applications
subdirectory:

## Home Work

This is a simple application where communities are created from pre-specified
distributions. The individual agents in these communities spend some portion of
every day at their places of work interacting with colleagues, and the rest of
their day at home interacting with household members. You can build and run the
example as follows.

### Native build (Linux)

```shell
bazel build -c opt agent_based_epidemic_sim/applications/home_work:main

bazel-bin/agent_based_epidemic_sim/applications/home_work/main \
  --simulation_config_pbtxt_path=agent_based_epidemic_sim/applications/home_work/config.pbtxt \
  --output_file_path=$HOME/output.csv
```

### Docker

```shell
docker build -t $USER/abesim .

docker run -t --rm -w /root/agent_based_epidemic_sim \
  -v $PWD:/root/agent_based_epidemic_sim:cached \
  -v /tmp/output:/tmp/output:delegated \
  -v /tmp/bazel_output:/tmp/bazel_output:delegated \
  $USER/abesim \
  bazel run -c opt agent_based_epidemic_sim/applications/home_work/main -- \
  --simulation_config_pbtxt_path=/root/agent_based_epidemic_sim/agent_based_epidemic_sim/applications/home_work/config.pbtxt \
  --output_file_path=/tmp/output/output.csv
```

## Contact Tracing

The contact tracing application is designed to investigate the effects of
different contact tracing strategies, including digital complements to contact
tracing, on the spread of disease. Work on this application is ongoing and it is
not yet ready to run.

## Configuration Generator

There is a configuration generator which pulls data from the
[COVID-19 Open Data repository][3] to seed the simulations with empirical data
for a given time and location. For example, to generate a config file for the
home-work application using the known number of infections, recovered and
deceased persons from Spain on May 1, run the following command:
```shell
python3 agent_based_epidemic_sim/configuration_generator/config.py \
  --region-code ES --date 2020-05-01 --sim home_work config.pbtxt
```

## Tests

### Native build (Linux)
```shell
bazel test agent_based_epidemic_sim/...
```

### Docker
```shell
docker build -t $USER/abesim .

docker run -t --rm -w /root/agent_based_epidemic_sim \
  -v $PWD:/root/agent_based_epidemic_sim:cached  \
  -v /tmp/bazel_output:/tmp/bazel_output:delegated \
  $USER/abesim bazel test agent_based_epidemic_sim/...
```

### Configuration Generator
The tests must be run from within the `configuration_generator` folder:
```shell
cd agent_based_epidemic_sim/configuration_generator
python3 -m unittest
```

## Risk Score Learning
Jupyter Notebooks for learning risk score models on data sampled from a
discrete grid on \[Infectiousness x Distance x Duration\] are available under
`agent_based_epidemic_sim/learning`. Notebook to reproduce the experiments
in [Murphy et al, Risk score learning for COVID-19 contact tracing apps,
2021][5] is available at
`agent_based_epidemic_sim/learning/MLHC_paper_experiments.ipynb`.

## Acknowledgements

The computational model used by the simulator is inspired in part by the
[EpiSimdemics simulator][1].  We also took a great deal of inspiration from the
[OpenABM simulator][2].

EpiSimdemics and its extensions were built by a team of researchers, the core
team members are now at UVA, UMD, UIUC, and LLNL. All the members that
contributed to EpiSimdemics and its extensions  include: Ashwin Aji,
Christopher L. Barrett, Abhinav Bhatele, Keith R. Bisset, Ali Butt,
Eric Bohm, Abhishek Gupta, Stephen G. Eubank, Xizhou Feng, Wu Feng,
Nikhil Jain, Laxmikant V. Kale, Tariq Kamal,  Chris Kuhlman, Yarden Livnat,
Madhav Marathe, Dimitrios S. Nikolopoulos, Martin Schulz, Lukasz Wesolowski,
Jae-Seung Yeom. Madhav Marathe is the corresponding scientist for EpiSimdemics
and can be reached at [marathe@virginia.edu][4].

[1]: http://charm.cs.uiuc.edu/research/episim
[2]: https://github.com/BDI-pathogens/OpenABM-Covid19
[3]: https://github.com/GoogleCloudPlatform/covid-19-open-data
[4]: mailto:marathe@virginia.edu
[5]: https://arxiv.org/abs/2104.08415
