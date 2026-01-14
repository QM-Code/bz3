#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
root_dir="$(cd "${script_dir}/.." && pwd)"
export PYTHONPATH="${root_dir}"

if [[ $# -ne 1 ]]; then
  echo "Usage: $0 /path/to/data.json"
  exit 1
fi

python3 -m bz3web.tools.import_data "$1"
