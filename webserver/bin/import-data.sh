#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
root_dir="$(cd "${script_dir}/.." && pwd)"
export PYTHONPATH="${root_dir}"

usage() {
  echo "Usage: $0 [-m|--merge] [-o|--overwrite] /path/to/data.json"
  exit 1
}

merge_flag=""
overwrite_flag=""
data_path=""

while [[ $# -gt 0 ]]; do
  case "$1" in
    -m|--merge)
      merge_flag="--merge"
      shift
      ;;
    -o|--overwrite)
      overwrite_flag="--overwrite"
      shift
      ;;
    -h|--help)
      usage
      ;;
    -*)
      usage
      ;;
    *)
      if [[ -n "${data_path}" ]]; then
        usage
      fi
      data_path="$1"
      shift
      ;;
  esac
done

if [[ -z "${data_path}" ]]; then
  usage
fi

python3 -m bz3web.tools.import_data ${merge_flag:+$merge_flag} ${overwrite_flag:+$overwrite_flag} "$data_path"
