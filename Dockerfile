# This file defines a docker container that can be used to build and test the
# simulator.

FROM ubuntu:20.04

RUN apt-get -y update
RUN apt-get -y upgrade
RUN apt-get --no-install-recommends -y install ca-certificates clang curl gnupg
RUN echo "deb [arch=amd64] http://storage.googleapis.com/bazel-apt stable jdk1.8" | tee /etc/apt/sources.list.d/bazel.list \
  && curl https://bazel.build/bazel-release.pub.gpg | apt-key add -
RUN apt-get -y update
RUN apt-get --no-install-recommends -y install bazel python3 python-is-python3 python3-distutils
RUN printf "startup --output_user_root=/tmp/bazel_output\n" > /root/.bazelrc
