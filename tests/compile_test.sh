#!/usr/bin/env bash
# Compile a single .c test file with mgcc, run it (if expected to succeed),
# and report PASS/FAIL.
#
# Usage: compile_test.sh <src.c> [extra mgcc flags...]
#
# Optional companion file <src.c>.compstatus:
#   Contains the integer exit status the compiler is expected to return.
#   - If the actual status does not match, the test FAILs.
#   - If the expected status is non-zero, no executable is expected and
#     the test is not run (a missing executable is not an error).
#   - If the file is absent, the compiler is expected to succeed (exit 0).
#
# Optional companion file <src.c>.compstderr:
#   Reference for the compiler's stderr output. After compilation, the
#   actual stderr is compared to this file (exact match). On mismatch the
#   test FAILs and the diff is shown. If the file is absent, stderr is not
#   checked (same behaviour as before).
#
# Optional companion files for the running program (only checked when the
# compiler succeeds and an executable is produced):
#   <src.c>.runstatus   integer exit status the program is expected to
#                       return. If absent, the program is expected to exit 0.
#   <src.c>.runstdout   reference for the program's stdout (exact match).
#   <src.c>.runstderr   reference for the program's stderr (exact match).
#   For .runstdout/.runstderr: on mismatch the test FAILs and the diff is
#   shown. If a file is absent, that stream is not checked.
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
compstatus_file="$src.compstatus"
compstderr_file="$src.compstderr"
runstatus_file="$src.runstatus"
runstdout_file="$src.runstdout"
runstderr_file="$src.runstderr"

expected_status=0
if [ -f "$compstatus_file" ]; then
    expected_status=$(head -1 "$compstatus_file" | tr -d '[:space:]')
    if ! echo "$expected_status" | grep -Eq '^[0-9]+$'; then
        echo "FAIL   $src (invalid .compstatus: '$expected_status')"
        exit 1
    fi
fi

mkdir -p "$OUTDIR"
rm -f "$exe"

"$MGCC" -o "$exe" "$src" "$@" >"$exe.out" 2>"$exe.err"
actual_status=$?

if [ "$actual_status" -ne "$expected_status" ]; then
    echo "FAIL   $src (compiler exit $actual_status, expected $expected_status)"
    cat "$exe.err"
    exit 1
fi

if [ -f "$compstderr_file" ]; then
    if ! diff -q "$compstderr_file" "$exe.err" >/dev/null; then
        echo "FAIL   $src (compiler stderr does not match $compstderr_file)"
        diff "$compstderr_file" "$exe.err" || true
        exit 1
    fi
fi

if [ "$expected_status" -ne 0 ]; then
    echo "PASS   $src (compile status $actual_status as expected, not run)"
    exit 0
fi

if [ ! -f "$exe" ]; then
    echo "FAIL   $src (compiler succeeded but no executable produced)"
    cat "$exe.err"
    exit 1
fi

"$exe" >"$exe.runout" 2>"$exe.runerr"
run_status=$?

expected_run_status=0
if [ -f "$runstatus_file" ]; then
    expected_run_status=$(head -1 "$runstatus_file" | tr -d '[:space:]')
    if ! echo "$expected_run_status" | grep -Eq '^[0-9]+$'; then
        echo "FAIL   $src (invalid .runstatus: '$expected_run_status')"
        exit 1
    fi
fi

if [ "$run_status" -ne "$expected_run_status" ]; then
    echo "FAIL   $src (run exit $run_status, expected $expected_run_status)"
    cat "$exe.runout"
    cat "$exe.runerr"
    exit 1
fi

if [ -f "$runstdout_file" ]; then
    if ! diff -q "$runstdout_file" "$exe.runout" >/dev/null; then
        echo "FAIL   $src (run stdout does not match $runstdout_file)"
        diff "$runstdout_file" "$exe.runout" || true
        exit 1
    fi
fi

if [ -f "$runstderr_file" ]; then
    if ! diff -q "$runstderr_file" "$exe.runerr" >/dev/null; then
        echo "FAIL   $src (run stderr does not match $runstderr_file)"
        diff "$runstderr_file" "$exe.runerr" || true
        exit 1
    fi
fi

echo "PASS   $src"
exit 0
