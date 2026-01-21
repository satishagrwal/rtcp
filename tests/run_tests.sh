#!/bin/sh
# Simple test runner supporting multiple cases.
# Place test cases under `tests/cases/<name>/` with files:
#   offer.sdp  answer.sdp  expected_output.txt

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT_DIR" || exit 1

echo "Compiling sdp_bw.c..."
gcc -Wall -Wextra sdp_bw.c -o sdp_bw || { echo "compile failed"; exit 2; }

cases_dir=tests/cases
failures=0
total=0

if [ -d "$cases_dir" ] && [ "$(ls -A "$cases_dir")" ]; then
    for d in "$cases_dir"/*; do
        [ -d "$d" ] || continue
        total=$((total+1))
        echo "Running case: $(basename "$d")"
        ./sdp_bw "$d/offer.sdp" "$d/answer.sdp" > out.txt 2> err.txt
        rc=$?
        if [ $rc -ne 0 ]; then
            # if test provides expected_error.txt, compare stderr
            if [ -f "$d/expected_error.txt" ]; then
                if diff -u "$d/expected_error.txt" err.txt > /dev/null; then
                    echo "  PASS (expected error exit $rc)"
                    rm -f out.txt err.txt
                    continue
                else
                    echo "  FAIL (exit $rc) -> stderr did not match expected_error.txt"
                    echo "---- stderr ----"
                    sed -n '1,200p' err.txt
                    echo "---- expected_error.txt ----"
                    sed -n '1,200p' "$d/expected_error.txt"
                    failures=$((failures+1))
                    rm -f out.txt err.txt
                    continue
                fi
            fi

            echo "  FAIL (exit $rc) -> stderr:"
            sed -n '1,200p' err.txt
            failures=$((failures+1))
            rm -f out.txt err.txt
            continue
        fi

        if diff -u "$d/expected_output.txt" out.txt > /dev/null; then
            echo "  PASS"
            rm -f out.txt err.txt
        else
            echo "  FAIL -> see out.txt vs $d/expected_output.txt"
            failures=$((failures+1))
        fi
    done
    echo "-----"
    echo "Ran $total case(s): $((total-failures)) passed, $failures failed"
    [ $failures -eq 0 ] && exit 0 || exit 1
else
    # fallback to single-root test (existing single-case layout)
    echo "Running single test (fallback)..."
    ./sdp_bw offer.sdp answer.sdp > out.txt
    if diff -u tests/expected_output.txt out.txt > /dev/null; then
        echo "TEST PASS"
        rm -f out.txt
        exit 0
    else
        echo "TEST FAIL"
        echo "---- actual ----"
        cat out.txt
        echo "---- expected ----"
        cat tests/expected_output.txt
        exit 1
    fi
fi
