#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu
helper=$1
php=$2
work=$(mktemp -d)
trap 'rm -rf "$work"' 0
trap 'exit 1' HUP INT TERM
mkdir "$work/bin"
ln -s "$php" "$work/bin/php"
PATH="$work/bin:$PATH"
export PATH
printf '%s\n' '<?php echo "EXECUTION_MARKER";' > "$work/valid file.php"
printf '%s\n' '<?php function broken( {' > "$work/invalid.php"
output=$(sh "$helper" "$work/valid file.php" 2>&1)
case "$output" in
    *EXECUTION_MARKER*) exit 1 ;;
esac
if sh "$helper" "$work/invalid.php" >/dev/null 2>&1; then
    exit 1
fi
if sh "$helper" 'https://example.invalid/page.php' >/dev/null 2>&1; then
    exit 1
fi
if sh "$helper" "$work" >/dev/null 2>&1; then
    exit 1
fi
