# Copyright 2020 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Generate pbtxt configuration file from empirical data."""


import argparse
import json
import os
import pathlib
from typing import Any, Dict, List, TextIO
import urllib.error
import urllib.request

# Root folder for this project.
ROOT = pathlib.Path(os.path.dirname(__file__)) / ".."

# URLs of empirical COVID-19 related data.
C19_OPEN_DATA_API = "https://storage.googleapis.com/covid19-open-data/v2/{key}/main.json"
C19_OPEN_DATA_PROJECT = "https://github.com/GoogleCloudPlatform/covid-19-open-data"

# Used for documentation, defined at global scope to avoid indentation issues.
EXAMPLE = (
    "python3 config.py "
    "--region-code {key} --date {date} --sim {app} config.pbtxt"
)
DESCRIPTION = f"""
Generate pbtxt configuration file from empirical data. Example usages:

- Seed home/work sim parameters with Spain's population and infected persons on May 1:
  $ {EXAMPLE.format(key="ES", date="2020-05-01", app="home_work")}

- Seed home/work sim parameters with data from Washington state as of June 3:
  $ {EXAMPLE.format(key="US_WA", date="2020-06-03", app="home_work")}
"""


def read_covid_data(key: str, date: str) -> List[Dict[str, Any]]:
  """Download the data up to `date` for the given region code `key`."""

  try:
    req = urllib.request.Request(C19_OPEN_DATA_API.format(key=key))
    with urllib.request.urlopen(req) as fh:
      data = json.load(fh)
      columns = data["columns"]
      records = ({col: val for col, val in zip(columns, row)}
                 for row in data["data"])
      records = [record for record in records if record["date"] <= date]
      assert (
          records and records[-1]["date"] == date
      ), f"Unable to find record with date '{date}'"
      return records

  except urllib.error.URLError:
    raise ValueError(f"Unable to fetch data for region code '{key}'")


def estimate_parameters(records: List[Dict[str, Any]]) -> Dict[str, Any]:
  """Estimate simulation parameters with the provided empirical data."""

  latest = records[-1]

  # Use the last 15 days of data to compute some estimates.
  prev_15_days = records[-15:]

  # Population is a static covariate according to this dataset.
  population = latest["population"]

  # We approximate infectious as the cumulative number of confirmed cases over
  # the past 14 days, multiplied by 10 to account for asymptomatic, false
  # negatives and untested population.
  infectious_count = 10 * sum(
      (record["new_confirmed"] for record in prev_15_days[1:])
  )

  # We assume that the population not infectious + population deceased are no
  # longer susceptible. Use count of recovered persons where available, fall
  # back to previously confirmed otherwise.
  if latest["total_recovered"]:
    immune_count = latest["total_recovered"] + latest["total_deceased"]
  else:
    immune_count = min((record["total_confirmed"] for record in prev_15_days))

  # The number of susceptible people are those remaining not in any of the other
  # buckets.
  susceptible_count = population - immune_count - infectious_count

  return {
      "population_size": population,
      "population_infectious_ratio": infectious_count / population,
      "population_susceptible_ratio": susceptible_count / population,
      "population_recovered_ratio": immune_count / population,
  }


def write_config(fh_out: TextIO, template: str, **params) -> None:
  """Output the config resulting from applying `params` to the `template`."""

  with open(template, "r") as fh_in:
    content = fh_in.read()
    for var, val in params.items():
      content = content.replace("{{" + var + "}}", str(val))
    fh_out.write(content)


def main():
  """Main entrypoint for the script."""

  # Process command-line arguments.
  argparser = argparse.ArgumentParser(
      description=DESCRIPTION,
      formatter_class=argparse.RawTextHelpFormatter,
  )
  argparser.add_argument(
      "--region-code",
      type=str,
      metavar="CODE",
      required=True,
      help=f"Region code present in {C19_OPEN_DATA_PROJECT}",
  )
  argparser.add_argument(
      "--date",
      type=str,
      metavar="YYYY-MM-DD",
      required=True,
      help="Date in the format YYYY-MM-DD to use as the data source",
  )
  argparser.add_argument(
      "--sim",
      type=str,
      metavar="APPLICATION",
      required=True,
      help="Simulation to generate a configuration file for",
  )
  argparser.add_argument(
      "output-file",
      type=str,
      help="Path to the output pbtxt file",
  )
  args = argparser.parse_args()

  # Retrieve appropriate application template.
  templates_folder = ROOT / "configuration_generator" / "templates"
  app_tpl = templates_folder / f"{args.sim}.tpl.pbtxt"
  assert (
      app_tpl.exists()
  ), f"Template for sim application '{args.sim}' could not be found."

  # Read records from remote endpoint.
  records = read_covid_data(args.region_code, args.date)

  # Estimate the simulation parameters.
  params = estimate_parameters(records)

  # Write the configuration file with estimated parameters.
  with open(getattr(args, "output-file"), "w") as fh:
    write_config(fh, str(app_tpl), **params)


if __name__ == "__main__":
  main()
