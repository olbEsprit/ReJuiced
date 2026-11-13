#!/bin/sh
# Lifts work/Juiced.exe (tools/prepare-exe.sh) to C++ in generated/, with the lifter of vendor/x86recomp.
set -eu
cd "$(dirname "$0")/.."
x=vendor/x86recomp
cmake -S $x -B $x/out/build/release -G Ninja -DCMAKE_BUILD_TYPE=Release >/dev/null
cmake --build $x/out/build/release --target recomp_lift >/dev/null
$x/out/build/release/tools/lifter_x86/recomp_lift --exe work/Juiced.exe --symbols symbols \
  --manifest manifest/juiced.manifest --out generated > work/lift.log 2>&1 || { tail work/lift.log; exit 1; }
tail -1 work/lift.log
