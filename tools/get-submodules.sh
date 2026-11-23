#!/usr/bin/env bash
# Fetches the submodules: the kernel (atmosphere-smp), the Verus inside it, and cFS.
#
# Verus is needed only as the source of vstd, the library the kernel is built
# against, so a shallow clone of it is enough. A recursive submodule update
# works too, but fetches the whole history of Verus.
set -euo pipefail
cd "$(dirname "$0")/.."
git submodule update --init atmosphere-smp
git -C atmosphere-smp submodule update --init --depth 1 verus
git submodule update --init --recursive cFS
