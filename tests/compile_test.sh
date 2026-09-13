#!/usr/bin/env bash
# Compile a single .c test file with mgcc, run it, and report PASS/FAIL.
#
# Usage: compile_test.sh <src.c> [extra mgcc flags...]
#
# The test passes when mgcc compiles the file, the resulting executable
# runs, and the executable exits with status 0.
set -u

MGCC=${MGCC:-./mgcc}
OUTDIR=${OUTDIR:-./tests/build}

src="$1"
shift || true

if [ ! -f "$src" ]; then
    echo "FAIL   $src (source not found)"
    exit 1
fi

name=$(basename "$src" .c)
exe="$OUTDIR/$name"

mkdir -p "$OUTDIR"

rm -f "$exe"

if ! "$MGCC" -o "$exe" "$src" "$@" >"$exe.log" 2>&1; then
    echo "FAIL   $src (compilation failed)"
    cat "$exe.log"
    exit 1
fi

if ! "$exe" >"$exe.run" 2>&1; then
    rc=$?
    echo "FAIL   $src (run exited $rc)"
    cat "$exe.run"
    exit 1
fi

echo "PASS   $src"
exit 0
