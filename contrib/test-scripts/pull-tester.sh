#!/bin/sh
# Helper script for pull-tester.
# _must_ be run from srcroot

./autogen.sh
./configure
./contrib/test-scripts/build-tests.sh "$@"
