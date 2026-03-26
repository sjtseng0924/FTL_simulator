#!/usr/bin/env bash
set -euo pipefail

TAR_FILE="${1:-systor17-01.tar}"
LUNS=(3 4 6)

if [[ ! -f "$TAR_FILE" ]]; then
    echo "tar file not found: $TAR_FILE" >&2
    exit 1
fi

for lun in "${LUNS[@]}"; do
    output="LUN${lun}.csv"
    echo "building ${output} from ${TAR_FILE} ..."

    mapfile -t members < <(tar -tf "$TAR_FILE" | grep -E -- "-LUN${lun}\.csv\.gz$" | sort)
    if [[ ${#members[@]} -eq 0 ]]; then
        echo "skip LUN${lun}: no matching files"
        continue
    fi

    : > "$output"
    first_file=1

    for member in "${members[@]}"; do
        echo "  adding ${member}"
        if [[ $first_file -eq 1 ]]; then
            tar -xOf "$TAR_FILE" "$member" | gzip -cd > "$output"
            first_file=0
        else
            tar -xOf "$TAR_FILE" "$member" | gzip -cd | tail -n +2 >> "$output"
        fi
    done
done

echo "done"
