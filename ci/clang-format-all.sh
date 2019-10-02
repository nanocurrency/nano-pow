#!/bin/bash

set -e

REPO_ROOT=$(git rev-parse --show-toplevel)
cd "${REPO_ROOT}"
./ci/update-clang-format
find include src -iname '*.h' -o -iname '*.hpp' -o -iname '*.cpp' -o -iname '*.cl' | xargs clang-format -i
