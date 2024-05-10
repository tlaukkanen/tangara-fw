#!/bin/bash
set -eu

if [ -z "${2:-}" ]; then
  name="Generic"
  locale="iso14651_t1"
else
  name="$1"
  locale="$2"
fi

working_dir=$(mktemp -d)

echo -n "$name" | dd iflag=fullblock bs=8 count=1 conv=sync of="$working_dir/name"
localedef -f UTF-8 -i "$locale" --no-archive "$working_dir" || true
cat "$working_dir/name" "$working_dir/LC_COLLATE" > "$name.LC_COLLATE"
