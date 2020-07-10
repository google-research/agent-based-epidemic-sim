# Contributing guidelines

## Pull Request Checklist

Before sending your pull requests, make sure you followed this list.

-   Read [contributing guidelines](CONTRIBUTING.md).
-   Read [Code of Conduct](CODE_OF_CONDUCT.md).
-   Ensure you have signed the
    [Contributor License Agreement (CLA)](https://cla.developers.google.com/).
-   Check if my changes are consistent with the
    [guidelines](https://github.com/google-research/agent-based-epidemic-sim/blob/develop/CONTRIBUTING.md#general-guidelines-and-philosophy-for-contribution).
-   Changes are consistent with the
    [Coding Style](https://github.com/google-research/agent-based-epidemic-sim/blob/develop/CONTRIBUTING.md#c-coding-style).
-   Run
    [Unit Tests](https://github.com/google-research/agent-based-epidemic-sim/blob/develop/CONTRIBUTING.md#general-guidelines-and-philosophy-for-contribution).

## How to become a contributor and submit your own code

### Contributor License Agreements

We'd love to accept your patches! Before we can take them, we have to jump a
couple of legal hurdles.

Please fill out either the individual or corporate Contributor License Agreement
(CLA).

*   If you are an individual writing original source code and you're sure you
    own the intellectual property, then you'll need to sign an
    [individual CLA](https://code.google.com/legal/individual-cla-v1.0.html).
*   If you work for a company that wants to allow you to contribute your work,
    then you'll need to sign a
    [corporate CLA](https://code.google.com/legal/corporate-cla-v1.0.html).

Follow either of the two links above to access the appropriate CLA and
instructions for how to sign and return it. Once we receive it, we'll be able to
accept your pull requests.

***NOTE***: Only original source code from you and other people that have signed
the CLA can be accepted into the main repository.

### Contributing code

If you have improvements to Agent Based Epidemic Sim, send us your pull
requests! For those just getting started, GitHub has a
[howto](https://help.github.com/articles/using-pull-requests/).

Agent Based Epidemic Sim team members will be assigned to review your pull
requests. Once the pull requests are approved and pass continuous integration
checks, a Agent Based Epidemic Sim team member will apply `ready to pull` label
to your change. This means we are working on getting your pull request submitted
to our internal repository. After the change has been submitted internally, your
pull request will be merged automatically on GitHub.

If you want to contribute but you're not sure where to start, take a look at the
[issues with the "contributions welcome" label](https://github.com/google-research/agent-based-epidemic-sim/labels/stat%3Acontributions%20welcome).
These are issues that we believe are particularly well suited for outside
contributions, often because we probably won't get to them right now. If you
decide to start on an issue, leave a comment so that other people know that
you're working on it. If you want to help out, but not alone, use the issue
comment thread to coordinate.

### Contribution guidelines and standards

Before sending your pull request for
[review](https://github.com/google-research/agent-based-epidemic-sim/pulls), make sure
your changes are consistent with the guidelines and follow the Agent Based
Epidemic Sim coding style.

#### General guidelines and philosophy for contribution

*   Include unit tests when you contribute new features, as they help to a)
    prove that your code works correctly, and b) guard against future breaking
    changes to lower the maintenance cost.
*   Bug fixes also generally require unit tests, because the presence of bugs
    usually indicates insufficient test coverage.
*   Keep API compatibility in mind when you change code in the Agent Based
    Epidemic Sim framework e.g., code in
    [agent_based_epidemic_sim/framework](https://github.com/google-research/agent-based-epidemic-sim/tree/develop/agent_based_epidemic_sim).
    Once Agent Based Epidemic Sim has reached version 1 we will not make
    non-backward-compatible API changes without a major release. Reviewers of
    your pull request will comment on any API compatibility issues.
*   When you contribute a new feature to Agent Based Epidemic Sim, the
    maintenance burden is (by default) transferred to the Agent Based Epidemic
    Sim team. This means that benefit of the contribution must be compared
    against the cost of maintaining the feature.

#### License

Include a license at the top of new files.

Bazel BUILD files also need to include a license section.

#### C++ coding style

Changes to Agent Based Epidemic Sim C++ code should conform to
[Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html).

Use `clang-tidy` to check your C/C++ changes. To install `clang-tidy` on
ubuntu:16.04, do:

```bash
apt-get install -y clang-tidy
```

You can check a C/C++ file by doing:

```bash
clang-format <my_cc_file> --style=google > /tmp/my_cc_file.cc
diff <my_cc_file> /tmp/my_cc_file.cc
```

#### Coding style for other languages

*   [Google Shell Style Guide](https://google.github.io/styleguide/shell.xml)
