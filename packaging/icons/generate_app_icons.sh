#!/usr/bin/env bash

set -euo pipefail

icon_root="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source_svg="${icon_root}/better_pull_review.svg"

for required_tool in sips magick iconutil; do
    if ! command -v "${required_tool}" >/dev/null 2>&1; then
        echo "generate_app_icons: missing required tool: ${required_tool}" >&2
        exit 1
    fi
done

working_directory="$(mktemp -d "${TMPDIR:-/tmp}/bpr-app-icon.XXXXXX")"
trap 'rm -rf "${working_directory}"' EXIT

rendered_source="${working_directory}/better_pull_review.png"
sips -s format png "${source_svg}" --out "${rendered_source}" >/dev/null

sizes=(16 24 32 40 48 64 128 256 512 1024)
for size in "${sizes[@]}"; do
    destination_directory="${icon_root}/png/${size}x${size}"
    mkdir -p "${destination_directory}"
    sips \
        -z "${size}" "${size}" \
        "${rendered_source}" \
        --out "${destination_directory}/better_pull_review.png" \
        >/dev/null
done

mkdir -p "${icon_root}/windows"
magick \
    "${icon_root}/png/16x16/better_pull_review.png" \
    "${icon_root}/png/24x24/better_pull_review.png" \
    "${icon_root}/png/32x32/better_pull_review.png" \
    "${icon_root}/png/40x40/better_pull_review.png" \
    "${icon_root}/png/48x48/better_pull_review.png" \
    "${icon_root}/png/64x64/better_pull_review.png" \
    "${icon_root}/png/128x128/better_pull_review.png" \
    "${icon_root}/png/256x256/better_pull_review.png" \
    "${icon_root}/windows/better_pull_review.ico"

iconset="${working_directory}/better_pull_review.iconset"
mkdir -p "${iconset}" "${icon_root}/macos"
cp "${icon_root}/png/16x16/better_pull_review.png" "${iconset}/icon_16x16.png"
cp "${icon_root}/png/32x32/better_pull_review.png" "${iconset}/icon_16x16@2x.png"
cp "${icon_root}/png/32x32/better_pull_review.png" "${iconset}/icon_32x32.png"
cp "${icon_root}/png/64x64/better_pull_review.png" "${iconset}/icon_32x32@2x.png"
cp "${icon_root}/png/128x128/better_pull_review.png" "${iconset}/icon_128x128.png"
cp "${icon_root}/png/256x256/better_pull_review.png" "${iconset}/icon_128x128@2x.png"
cp "${icon_root}/png/256x256/better_pull_review.png" "${iconset}/icon_256x256.png"
cp "${icon_root}/png/512x512/better_pull_review.png" "${iconset}/icon_256x256@2x.png"
cp "${icon_root}/png/512x512/better_pull_review.png" "${iconset}/icon_512x512.png"
cp "${icon_root}/png/1024x1024/better_pull_review.png" "${iconset}/icon_512x512@2x.png"
iconutil \
    --convert icns \
    "${iconset}" \
    --output "${icon_root}/macos/better_pull_review.icns"

echo "generate_app_icons: regenerated PNG, ICO, and ICNS assets"
