#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
EXPECTED_REPOSITORY="MikeMarcin/BetterPullReview"
EXPECTED_VISIBILITY="PRIVATE"
DRY_RUN=0
PRERELEASE=0
NOTES_FILE=""
VERSION=""
PREPARE_CANDIDATE=0
FREEZE_CANDIDATE=""
CANDIDATE_INPUTS=""
COHORT_OUTPUT=""
RELEASE_COHORT=""
MANIFEST_ARGS=()
ASSETS=()
INCLUDE_MACOS=0
APPEND_ASSETS=0
PROMOTE=0
SOURCE_REVISION=""
BASELINE_INVENTORY=""
PROMOTION_ACCEPTANCE=""
MACOS_DIR=""

usage() {
    cat <<EOF
Candidate preparation: --prepare-candidate --candidate-inputs ABS.json --cohort-output ABS.json
After committing exactly the counter/receipt: --freeze-candidate N --cohort-output ABS.json

Usage: tools/publish-release.sh --version VERSION --notes-file PATH --cohort ABS.json [--manifest PATH ...] [--macos] [--public] [--prerelease] [--dry-run]
       tools/publish-release.sh --append-assets|--promote --version VERSION --source-revision SHA --baseline-inventory PATH --notes-file PATH [--manifest PATH ...] [--macos-dir PATH] [--promotion-acceptance PATH] [--public] [--dry-run]

Publish already verified native artifacts to the expected GitHub repository.
Without --manifest, use the standard macOS eight-asset set. --macos adds that
set to explicitly selected Windows/Linux manifests. Offline checks bind bytes
and producer receipts; native trust and release approval remain separate gates.
The command refuses to push a tag or create a release unless GitHub reports
${EXPECTED_REPOSITORY} with the selected visibility immediately before each
remote mutation. The default requires PRIVATE; --public requires PUBLIC.
--public never changes repository visibility. origin must have one canonical
GitHub HTTPS or SSH push URL for ${EXPECTED_REPOSITORY}.
--prerelease marks the GitHub release as a prerelease and keeps it out of Latest.
--append-assets adds verified missing assets to an existing mutable prerelease;
existing bytes and the annotated source tag must match the supplied baseline.
--promote requires the complete standard set and explicit stable qualification.
These modes require --source-revision and permit only a separately committed
publisher/test tool revision above that exact producer source. They never retag,
replace assets or change repository settings. Native trust remains separate.
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

repository_guard() {
    local observed push_url
    push_url="$(git -C "${ROOT_DIR}" remote get-url --push --all origin)"
    case "${push_url}" in
        "https://github.com/${EXPECTED_REPOSITORY}"|"https://github.com/${EXPECTED_REPOSITORY}.git"|\
        "git@github.com:${EXPECTED_REPOSITORY}"|"git@github.com:${EXPECTED_REPOSITORY}.git"|\
        "ssh://git@github.com/${EXPECTED_REPOSITORY}"|"ssh://git@github.com/${EXPECTED_REPOSITORY}.git") ;;
        *) fail "origin must have one canonical GitHub push URL for ${EXPECTED_REPOSITORY}" ;;
    esac
    observed="$(gh repo view "${EXPECTED_REPOSITORY}" \
        --json nameWithOwner,visibility \
        --jq '.nameWithOwner + "\t" + .visibility')"
    [[ "${observed}" == "${EXPECTED_REPOSITORY}"$'\t'"${EXPECTED_VISIBILITY}" ]] || \
        fail "GitHub repository identity/visibility does not match ${EXPECTED_REPOSITORY} (${EXPECTED_VISIBILITY})"
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --prepare-candidate) PREPARE_CANDIDATE=1; shift ;;
        --freeze-candidate) FREEZE_CANDIDATE="$2"; shift 2 ;;
        --candidate-inputs) CANDIDATE_INPUTS="$2"; shift 2 ;;
        --cohort-output) COHORT_OUTPUT="$2"; shift 2 ;;
        --cohort) RELEASE_COHORT="$2"; shift 2 ;;
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
        --append-assets)
            APPEND_ASSETS=1
            shift
            ;;
        --promote)
            PROMOTE=1
            shift
            ;;
        --source-revision|--baseline-inventory|--promotion-acceptance|--macos-dir)
            [[ $# -ge 2 ]] || fail "$1 requires a value"
            case "$1" in
                --source-revision) SOURCE_REVISION="$2" ;;
                --baseline-inventory) BASELINE_INVENTORY="$2" ;;
                --promotion-acceptance) PROMOTION_ACCEPTANCE="$2" ;;
                --macos-dir) MACOS_DIR="$2" ;;
            esac
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
        --public)
            EXPECTED_VISIBILITY="PUBLIC"
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

if [[ "${PREPARE_CANDIDATE}" == 1 || -n "${FREEZE_CANDIDATE}" ]]; then
    [[ -n "${COHORT_OUTPUT}" && "${COHORT_OUTPUT}" == /* ]] || fail "Candidate output must be an absolute external path"
    if [[ "${PREPARE_CANDIDATE}" == 1 ]]; then
        [[ -z "${FREEZE_CANDIDATE}" && -n "${CANDIDATE_INPUTS}" ]] || fail "Preparation needs one complete candidate-inputs JSON"
        # One controller reconciles authoritative main before reserving from another clone.
        git -C "${ROOT_DIR}" fetch --no-tags origin main
        git -C "${ROOT_DIR}" merge-base --is-ancestor origin/main HEAD || fail "Reconcile authoritative main before allocating"
        python3 "${ROOT_DIR}/packaging/release_build.py" allocate --root "${ROOT_DIR}" --inputs "${CANDIDATE_INPUTS}" --output "${COHORT_OUTPUT}"
    else
        python3 "${ROOT_DIR}/packaging/release_build.py" freeze --root "${ROOT_DIR}" --number "${FREEZE_CANDIDATE}" --output "${COHORT_OUTPUT}"
    fi
    exit 0
fi
[[ -z "${CANDIDATE_INPUTS}${COHORT_OUTPUT}" ]] || fail "Candidate preparation options require prepare/freeze"

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

[[ ! ("${APPEND_ASSETS}" == 1 && "${PROMOTE}" == 1) ]] || fail "Select append or promotion, not both"
if [[ "${APPEND_ASSETS}" == 1 || "${PROMOTE}" == 1 ]]; then
    [[ "${PRERELEASE}" == 0 ]] || fail "--prerelease applies to initial creation only"
    [[ -n "${SOURCE_REVISION}" && -n "${BASELINE_INVENTORY}" ]] || fail "Existing release updates require exact source and existing-asset baseline"
    UPDATE_ARGS=(--repo-root "${ROOT_DIR}" --version "${VERSION}" --source-revision "${SOURCE_REVISION}" --visibility "${EXPECTED_VISIBILITY}" --notes-file "${NOTES_FILE}" --baseline-inventory "${BASELINE_INVENTORY}")
    if [[ ${#MANIFEST_ARGS[@]} -gt 0 ]]; then UPDATE_ARGS+=("${MANIFEST_ARGS[@]}"); fi
    if [[ -n "${MACOS_DIR}" ]]; then
        UPDATE_ARGS+=(--macos-dir "${MACOS_DIR}")
    elif [[ "${INCLUDE_MACOS}" == 1 || ${#MANIFEST_ARGS[@]} -eq 0 ]]; then
        UPDATE_ARGS+=(--macos-dir "${ROOT_DIR}/build/releases/${VERSION}")
    fi
    if [[ -n "${RELEASE_COHORT}" ]]; then UPDATE_ARGS+=(--cohort "${RELEASE_COHORT}"); fi
    if [[ -n "${PROMOTION_ACCEPTANCE}" ]]; then UPDATE_ARGS+=(--promotion-acceptance "${PROMOTION_ACCEPTANCE}"); fi
    if [[ "${PROMOTE}" == 1 ]]; then UPDATE_ARGS+=(--promote); fi
    if [[ "${DRY_RUN}" == 1 ]]; then UPDATE_ARGS+=(--dry-run); fi
    python3 "${ROOT_DIR}/packaging/update_release.py" "${UPDATE_ARGS[@]}"
    exit 0
fi
[[ -z "${SOURCE_REVISION}${BASELINE_INVENTORY}${PROMOTION_ACCEPTANCE}${MACOS_DIR}" ]] || fail "Existing-release identity options require --append-assets or --promote"

[[ -n "${RELEASE_COHORT}" ]] || fail "Initial publication requires --cohort from publish-release candidate preparation"
python3 "${ROOT_DIR}/packaging/release_build.py" check --root "${ROOT_DIR}" --cohort "${RELEASE_COHORT}" --version "${VERSION}"
ASSET_VERSION="$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["allocation"]["native"]["assetVersion"])' "${RELEASE_COHORT}")"
TAG="v${VERSION}"
if [[ ${#MANIFEST_ARGS[@]} -eq 0 || "${INCLUDE_MACOS}" == "1" ]]; then
ASSET_BASE="BetterPullReview-${ASSET_VERSION}-macos-universal"
DIST_DIR="${ROOT_DIR}/build/releases/${ASSET_VERSION}"
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
    --source-archive "${SOURCE}" --version "${VERSION}" --source-revision "$(git -C "${ROOT_DIR}" rev-parse HEAD)" --cohort "${RELEASE_COHORT}" --legacy-zip "${DIST_DIR}/BetterPullReview-${VERSION}-macos-universal.zip"
ASSETS+=("${DIST_DIR}/BetterPullReview-${VERSION}-macos-universal.zip" "${DIST_DIR}/BetterPullReview-${VERSION}-macos-universal.zip.sha256" "${DMG}" "${ZIP}" "${DMG_SHA}" "${ZIP_SHA}" "${SBOM}" "${SBOM_SHA}" "${SOURCE}" "${SOURCE_SHA}")
fi
if [[ ${#MANIFEST_ARGS[@]} -gt 0 ]]; then
    asset_list="$(mktemp)"
    trap 'exit_status=$?; rm -f "${asset_list}"; exit "${exit_status}"' EXIT
    python3 "${ROOT_DIR}/packaging/verify_native_release.py" "${MANIFEST_ARGS[@]}" \
        --version "${VERSION}" --source-revision "$(git -C "${ROOT_DIR}" rev-parse HEAD)" --asset-list "${asset_list}" --cohort "${RELEASE_COHORT}"
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

repository_guard
git -C "${ROOT_DIR}" rev-parse --verify --quiet "refs/tags/${TAG}" >/dev/null && \
    fail "Local tag already exists: ${TAG}"
[[ -z "$(git -C "${ROOT_DIR}" ls-remote --tags origin "refs/tags/${TAG}")" ]] || \
    fail "Remote tag already exists: ${TAG}"
gh release view "${TAG}" --repo "${EXPECTED_REPOSITORY}" >/dev/null 2>&1 && \
    fail "GitHub release already exists: ${TAG}"

if [[ "${DRY_RUN}" == "1" ]]; then
    printf 'Release preflight passed for %s at %s (repository visibility: %s)\n' "${TAG}" "$(git -C "${ROOT_DIR}" rev-parse HEAD)" "${EXPECTED_VISIBILITY}"
    exit 0
fi

repository_guard
git -C "${ROOT_DIR}" tag -a "${TAG}" -m "${TAG}"
git -C "${ROOT_DIR}" push --atomic origin main "${TAG}"

repository_guard
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
