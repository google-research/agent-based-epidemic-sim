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

import os
import sys
import unittest

# This is necessary due to how relative imports work in Python, because we don't
# want to create a whole package just for a relatively simple script.
sys.path.append(os.path.abspath(os.path.join(__file__, "..")))
# pylint: disable=g-import-not-at-top
from config import read_covid_data


class TestConfigGenerator(unittest.TestCase):

  def _test_read_data_helper(self, key: str, date: str):
    records = read_covid_data(key, date)
    self.assertTrue(records, "Records are empty or null")

    latest = records[-1]
    self.assertEqual(latest["date"], date, "Record does not contain date")
    self.assertGreater(len(latest.keys()), 0, "Record contains no keys")
    self.assertGreater(
        len([x for x in latest.values() if x]), 0, "Record values are all null"
    )

  def test_read_covid_data(self):
    self._test_read_data_helper("ES", "2020-05-01")
    self._test_read_data_helper("US_WA", "2020-06-03")


if __name__ == "__main__":
  sys.exit(unittest.main())
