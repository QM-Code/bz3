#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
root_dir="$(cd "${script_dir}/.." && pwd)"
export PYTHONPATH="${root_dir}"

usage() {
  echo "Usage: $0 [-z|--zip] [/path/to/output.json]"
  exit 1
}

zip_output=false
output_path=""

while [[ $# -gt 0 ]]; do
  case "$1" in
    -z|--zip)
      zip_output=true
      shift
      ;;
    -h|--help)
      usage
      ;;
    -*)
      usage
      ;;
    *)
      if [[ -n "${output_path}" ]]; then
        usage
      fi
      output_path="$1"
      shift
      ;;
  esac
done

if [[ -z "${output_path}" ]]; then
  output_path="${root_dir}/data/snapshot-$(date +%Y%m%d-%H%M%S).json"
  echo "Exporting snapshot to ${output_path}"
fi

python3 -m bz3web.tools.export_data "$output_path"

if [[ "${zip_output}" == "true" ]]; then
  zip_path="${output_path%.json}.zip"
  zip -j "$zip_path" "$output_path" >/dev/null
  rm -f "$output_path"
  echo "Wrote ${zip_path}"
fi
