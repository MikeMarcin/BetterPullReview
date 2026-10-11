#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
EXPECTED_REPOSITORY="MikeMarcin/BetterPullReview"
DRY_RUN=0
PRERELEASE=0
NOTES_FILE=""
VERSION=""
MANIFEST_ARGS=()
ASSETS=()
INCLUDE_MACOS=0

usage() {
    cat <<EOF
Usage: tools/publish-release.sh --version VERSION --notes-file PATH [--manifest PATH ...] [--macos] [--prerelease] [--dry-run]

Publish already verified native artifacts to the private GitHub repository.
Without --manifest, use the standard macOS eight-asset set. --macos adds that
set to explicitly selected Windows/Linux manifests. Offline checks bind bytes
and producer receipts; native trust and release approval remain separate gates.
The command refuses to push a tag or create a release unless GitHub reports
${EXPECTED_REPOSITORY} as PRIVATE immediately before each remote mutation.
--prerelease marks the GitHub release as a prerelease and keeps it out of Latest.
EOF
}

fail() {
    printf 'error: %s\n' "$*" >&2
    exit 1
}

verify_checksums() {
    if command -v shasum >/dev/null 2>&1; then
        shasum -a 256 -c "$1"
    elif command -v sha256sum >/dev/null 2>&1; then
        sha256sum -c "$1"
    else
        fail "Required checksum tool not found: shasum or sha256sum"
    fi
}

private_repository_guard() {
    local observed
    observed="$(gh repo view "${EXPECTED_REPOSITORY}" \
        --json nameWithOwner,visibility \
        --jq '.nameWithOwner + "\t" + .visibility')"
    [[ "${observed}" == "${EXPECTED_REPOSITORY}"$'\tPRIVATE' ]] || \
        fail "GitHub repository is not the expected private repository: ${observed}"
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --version)
            [[ $# -ge 2 ]] || fail "--version requires a value"
            VERSION="${2#v}"
            shift 2
            ;;
        --notes-file)
            [[ $# -ge 2 ]] || fail "--notes-file requires a value"
            NOTES_FILE="$2"
            shift 2
            ;;
        --dry-run)
            DRY_RUN=1
            shift
            ;;
        --prerelease)
            PRERELEASE=1
            shift
            ;;
        --manifest)
            [[ $# -ge 2 ]] || fail "--manifest requires a value"
            manifest="$2"
            [[ "${manifest}" == /* ]] || manifest="${ROOT_DIR}/${manifest}"
            MANIFEST_ARGS+=(--manifest "${manifest}")
            shift 2
            ;;
        --macos)
            INCLUDE_MACOS=1
            shift
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            fail "Unknown option: $1"
            ;;
    esac
done

[[ "${VERSION}" =~ ^[0-9]+\.[0-9]+\.[0-9]+([.-][0-9A-Za-z.-]+)?$ ]] || \
    fail "--version must be a SemVer value"
[[ -n "${NOTES_FILE}" ]] || fail "--notes-file is required"
if [[ "${NOTES_FILE}" != /* ]]; then
    NOTES_FILE="${ROOT_DIR}/${NOTES_FILE}"
fi
[[ -f "${NOTES_FILE}" ]] || fail "Release notes are absent: ${NOTES_FILE}"
command -v gh >/dev/null 2>&1 || fail "Required command not found: gh"

[[ -z "$(git -C "${ROOT_DIR}" status --porcelain)" ]] || fail "Working tree is not clean"
[[ "$(git -C "${ROOT_DIR}" branch --show-current)" == "main" ]] || fail "Release must run from main"

PROJECT_VERSION="$(sed -n '/^project(/,/^)/p' "${ROOT_DIR}/CMakeLists.txt" |
    sed -n 's/^[[:space:]]*VERSION[[:space:]]\{1,\}\([^[:space:]]\{1,\}\).*/\1/p' |
    head -n 1)"
[[ "${PROJECT_VERSION}" == "${VERSION}" ]] || fail "CMake project version is ${PROJECT_VERSION}, not ${VERSION}"

TAG="v${VERSION}"
if [[ ${#MANIFEST_ARGS[@]} -eq 0 || "${INCLUDE_MACOS}" == "1" ]]; then
ASSET_BASE="BetterPullReview-${VERSION}-macos-universal"
DIST_DIR="${ROOT_DIR}/build/releases/${VERSION}"
DMG="${DIST_DIR}/${ASSET_BASE}.dmg"
ZIP="${DIST_DIR}/${ASSET_BASE}.zip"
DMG_SHA="${DMG}.sha256"
ZIP_SHA="${ZIP}.sha256"
SBOM="${DIST_DIR}/${ASSET_BASE}.spdx.json"
SBOM_SHA="${SBOM}.sha256"
SOURCE="${DIST_DIR}/Source-${ASSET_BASE}-corresponding-source.zip"
SOURCE_SHA="${SOURCE}.sha256"

for artifact in "${DMG}" "${ZIP}" "${DMG_SHA}" "${ZIP_SHA}" "${SBOM}" "${SBOM_SHA}" "${SOURCE}" "${SOURCE_SHA}"; do
    [[ -f "${artifact}" ]] || fail "Release artifact is absent: ${artifact}"
done
(
    cd "${DIST_DIR}"
    verify_checksums "$(basename "${DMG_SHA}")"
    verify_checksums "$(basename "${ZIP_SHA}")"
    verify_checksums "$(basename "${SBOM_SHA}")"
    verify_checksums "$(basename "${SOURCE_SHA}")"
)
python3 "${ROOT_DIR}/packaging/verify_release_sources.py" --app-zip "${ZIP}" --sbom "${SBOM}" \
    --source-archive "${SOURCE}" --version "${VERSION}" --source-revision "$(git -C "${ROOT_DIR}" rev-parse HEAD)"
ASSETS+=("${DMG}" "${ZIP}" "${DMG_SHA}" "${ZIP_SHA}" "${SBOM}" "${SBOM_SHA}" "${SOURCE}" "${SOURCE_SHA}")
fi
if [[ ${#MANIFEST_ARGS[@]} -gt 0 ]]; then
    asset_list="$(mktemp)"
    trap 'exit_status=$?; rm -f "${asset_list}"; exit "${exit_status}"' EXIT
    python3 "${ROOT_DIR}/packaging/verify_native_release.py" "${MANIFEST_ARGS[@]}" \
        --version "${VERSION}" --source-revision "$(git -C "${ROOT_DIR}" rev-parse HEAD)" --asset-list "${asset_list}"
    while IFS= read -r -d '' artifact; do
        # The Python gate has already rejected conflicting native basenames.
        # macOS ships Bash 3.2: expanding an empty array under nounset fails.
        if [[ ${#ASSETS[@]} -gt 0 ]]; then
            for existing in "${ASSETS[@]}"; do
                [[ "$(basename "${artifact}")" != "$(basename "${existing}")" ]] || fail "Asset name collides with macOS set"
            done
        fi
        ASSETS+=("${artifact}")
    done < "${asset_list}"
    [[ ${#ASSETS[@]} -gt 0 ]] || fail "No verified release assets"
fi

private_repository_guard
git -C "${ROOT_DIR}" rev-parse --verify --quiet "refs/tags/${TAG}" >/dev/null && \
    fail "Local tag already exists: ${TAG}"
[[ -z "$(git -C "${ROOT_DIR}" ls-remote --tags origin "refs/tags/${TAG}")" ]] || \
    fail "Remote tag already exists: ${TAG}"
gh release view "${TAG}" --repo "${EXPECTED_REPOSITORY}" >/dev/null 2>&1 && \
    fail "GitHub release already exists: ${TAG}"

if [[ "${DRY_RUN}" == "1" ]]; then
    printf 'Private release preflight passed for %s at %s\n' "${TAG}" "$(git -C "${ROOT_DIR}" rev-parse HEAD)"
    exit 0
fi

private_repository_guard
git -C "${ROOT_DIR}" tag -a "${TAG}" -m "${TAG}"
git -C "${ROOT_DIR}" push --atomic origin main "${TAG}"

private_repository_guard
set --
if [[ "${PRERELEASE}" == "1" ]]; then
    set -- --prerelease --latest=false
fi
gh release create "${TAG}" \
    "${ASSETS[@]}" \
    --repo "${EXPECTED_REPOSITORY}" \
    --title "BetterPullReview ${TAG}" \
    --notes-file "${NOTES_FILE}" \
    --verify-tag "$@"

gh release view "${TAG}" --repo "${EXPECTED_REPOSITORY}" \
    --json isDraft,isPrerelease,tagName,url
