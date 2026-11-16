#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp/root/alpha/nested" "$tmp/root/space dir"
touch "$tmp/root/alpha/file.txt" "$tmp/root/.hidden" "$tmp/root/skip.log"

out=$(./ctree --plain --noreport -L 1 "$tmp/root")
printf '%s\n' "$out" | grep -q 'alpha'
printf '%s\n' "$out" | grep -q 'space dir'
if printf '%s\n' "$out" | grep -q 'nested'; then exit 1; fi
if printf '%s\n' "$out" | grep -q '.hidden'; then exit 1; fi

out=$(./ctree --plain -a -d -L 2 "$tmp/root")
printf '%s\n' "$out" | grep -q 'nested'
if printf '%s\n' "$out" | grep -q 'file.txt'; then exit 1; fi

out=$(./ctree --plain -I '*.log' "$tmp/root")
if printf '%s\n' "$out" | grep -q 'skip.log'; then exit 1; fi

out=$(./ctree --plain -P '*.txt' "$tmp/root")
printf '%s\n' "$out" | grep -q 'file.txt'
if printf '%s\n' "$out" | grep -q 'skip.log'; then exit 1; fi

if ./ctree --plain -L 0 "$tmp/root" >/dev/null 2>&1; then exit 1; fi
python3 tests/pty_test.py "$tmp/root"
printf 'All tests passed\n'
