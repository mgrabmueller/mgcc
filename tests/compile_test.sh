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
#   - If the file is absent, the compiler is expected to succeed (exit 0)
#     and the resulting executable must run and exit 0.
#
# Optional companion file <src.c>.compstderr:
#   Reference for the compiler's stderr output. After compilation, the
#   actual stderr is compared to this file (exact match). On mismatch the
#   test FAILs and the diff is shown. If the file is absent, stderr is not
#   checked (same behaviour as before).
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

if ! "$exe" >"$exe.run" 2>&1; then
    rc=$?
    echo "FAIL   $src (run exited $rc)"
    cat "$exe.run"
    exit 1
fi

echo "PASS   $src"
exit 0
