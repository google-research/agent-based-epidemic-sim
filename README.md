# Agent Based Epidemic Simulator

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
example as follows:

```shell
bazel build -c opt agent_based_epidemic_sim/applications/home_work:main

bazel-bin/agent_based_epidemic_sim/applications/home_work/main \
  --simulation_config_pbtxt_path=agent_based_epidemic_sim/applications/home_work/config.pbtxt \
  --output_file_path=$HOME/output.csv
```

## Contact Tracing

The contact tracing application is designed to investigate the effects of
different contact tracing strategies, including digital complements to contact
tracing, on the spread of disease. Work on this application is ongoing and it is
not yet ready to run.

## Tests

To run tests:

```shell
bazel test agent_based_epidemic_sim/...
```

## Acknowledgements

The computational model used by the simulator is inspired in part by the
[EpiSimdemics simulator][1].  We also took a great deal of inspiration from the
[OpenABM simulator][2].

[1]: http://charm.cs.uiuc.edu/research/episim
[2]: https://github.com/BDI-pathogens/OpenABM-Covid19
