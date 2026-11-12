#!/bin/sh
# Checks out the x86recomp commit X86RECOMP pins into vendor/x86recomp.
set -eu
cd "$(dirname "$0")/.."
read -r branch sha < X86RECOMP
url=${X86RECOMP_URL:-https://github.com/olbEsprit/SwARc.git}
[ -d vendor/x86recomp/.git ] || git clone --branch "$branch" "$url" vendor/x86recomp
git -C vendor/x86recomp fetch origin "$branch"
git -C vendor/x86recomp checkout --detach "$sha"
