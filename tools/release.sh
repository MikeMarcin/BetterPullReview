#!/usr/bin/env bash
set -euo pipefail

APP_NAME="BetterPullReview"
BUNDLE_ID="com.gamecoretech.betterpullreview"
TEAM_ID="YTQKP2V2A8"
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# [[BPR-1179]]. --variant selects a package without a removable component family;
# the preset, build directory and artifact names follow it.
VARIANT=""
DEFAULT_CODESIGN_IDENTITY="Developer ID Application: Michael Marcin (${TEAM_ID})"
DEFAULT_NOTARY_PROFILE="BetterPullReview-notary"

CODESIGN_IDENTITY="${APPLE_CODESIGN_IDENTITY:-${DEFAULT_CODESIGN_IDENTITY}}"
NOTARY_PROFILE="${APPLE_NOTARY_PROFILE:-${DEFAULT_NOTARY_PROFILE}}"
NOTARY_KEYCHAIN="${APPLE_NOTARY_KEYCHAIN:-}"
ALLOW_DIRTY=0
SKIP_NOTARIZE=0
SKIP_TESTS=0
PREPARE_ONLY=0
OUTPUT_DIR=""
REQUESTED_VERSION=""

usage() {
    cat <<EOF
Usage: tools/release.sh --version VERSION [options]

Build, deploy, sign, notarize, and package the universal macOS application.

Options:
  --version VERSION       Required SemVer; must match the CMake project version.
  --identity NAME         Developer ID Application identity.
  --notary-profile NAME  notarytool Keychain profile (default: ${DEFAULT_NOTARY_PROFILE}).
  --notary-keychain PATH Existing file Keychain for the selected profile.
  --output-dir DIR       Artifact directory (default: build/releases/VERSION).
  --skip-tests           Skip the dev build and test preset.
  --skip-notarize        Build signed artifacts without submitting them to Apple.
  --prepare-only        Local deployment, notices/source, audit and ZIP only;
                        ad-hoc seal, no Keychain lookup or Apple submissions.
  --allow-dirty          Permit uncommitted source changes.
  --variant NAME         agent-free (no agent integrations) or
                         agent-free-automation-off (no local automation either);
                         artifacts are named for it. Default: the full package.
  -h, --help             Show this help.

Environment:
  APPLE_CODESIGN_IDENTITY  Default identity for --identity.
  APPLE_NOTARY_PROFILE     Default profile for --notary-profile.
  APPLE_NOTARY_KEYCHAIN    Default path for --notary-keychain (optional).
  MACDEPLOYQT              Exact macdeployqt executable to use.
EOF
}

fail() {
    printf 'error: %s\n' "$*" >&2
    exit 1
}

log() {
    printf '==> %s\n' "$*"
}

require_command() {
    command -v "$1" >/dev/null 2>&1 || fail "Required command not found: $1"
}

notary_submit() {
    local artifact="$1"
    if [[ -n "${NOTARY_KEYCHAIN}" ]]; then
        xcrun notarytool submit "${artifact}" --keychain-profile "${NOTARY_PROFILE}" --keychain "${NOTARY_KEYCHAIN}" --wait
    else
        xcrun notarytool submit "${artifact}" --keychain-profile "${NOTARY_PROFILE}" --wait
    fi
}

project_version() {
    sed -n '/^project(/,/^)/p' "${ROOT_DIR}/CMakeLists.txt" |
        sed -n 's/^[[:space:]]*VERSION[[:space:]]\{1,\}\([^[:space:]]\{1,\}\).*/\1/p' |
        head -n 1
}

plist_value() {
    /usr/libexec/PlistBuddy -c "Print :$1" "$2"
}

is_mach_o() {
    file -b "$1" | grep -q 'Mach-O'
}

sign_mach_file() {
    codesign --force --timestamp --options runtime --sign "${CODESIGN_IDENTITY}" "$1"
}

sign_nested_code() {
    local bundle="$1"
    local framework
    local binary

    while IFS= read -r -d '' framework; do
        sign_mach_file "${framework}"
    done < <(find "${bundle}/Contents/Frameworks" -type d -name '*.framework' -print0)

    while IFS= read -r -d '' binary; do
        if is_mach_o "${binary}" && [[ "${binary}" != *'.framework/'* ]]; then
            sign_mach_file "${binary}"
        fi
    done < <(find "${bundle}/Contents/Frameworks" "${bundle}/Contents/PlugIns" \
        "${bundle}/Contents/MacOS" -type f -print0)

    codesign --force --timestamp --options runtime \
        --sign "${CODESIGN_IDENTITY}" "${bundle}"
}

verify_plugin_policy() {
    local bundle="$1"
    local plugins="${bundle}/Contents/PlugIns"
    local allowed=(
        "networkinformation/libqapplenetworkinformation.dylib"
        "platforms/libqcocoa.dylib"
        "styles/libqmacstyle.dylib"
        "tls/libqsecuretransportbackend.dylib"
    )
    local relative
    local expected

    [[ -d "${plugins}" ]] || fail "macdeployqt did not create ${plugins}"
    while IFS= read -r -d '' relative; do
        relative="${relative#"${plugins}"/}"
        for expected in "${allowed[@]}"; do
            if [[ "${relative}" == "${expected}" ]]; then
                continue 2
            fi
        done
        rm -f "${plugins}/${relative}"
    done < <(find "${plugins}" \( -type f -o -type l \) -print0)

    find "${plugins}" -depth -type d -empty -delete
    for expected in "${allowed[@]}"; do
        [[ -f "${plugins}/${expected}" ]] || fail "Required Qt plugin is absent: ${expected}"
    done
}

verify_framework_policy() {
    local bundle="$1"
    local frameworks="${bundle}/Contents/Frameworks"
    local allowed=(
        "QtCore.framework"
        "QtDBus.framework"
        "QtGui.framework"
        "QtNetwork.framework"
        "QtWidgets.framework"
    )
    local framework
    local relative
    local expected

    [[ -d "${frameworks}" ]] || fail "macdeployqt did not create ${frameworks}"
    while IFS= read -r -d '' framework; do
        relative="${framework#"${frameworks}"/}"
        for expected in "${allowed[@]}"; do
            if [[ "${relative}" == "${expected}" ]]; then
                continue 2
            fi
        done
        rm -rf "${framework}"
    done < <(find "${frameworks}" -mindepth 1 -maxdepth 1 -type d -name '*.framework' -print0)

    for expected in "${allowed[@]}"; do
        [[ -d "${frameworks}/${expected}" ]] || fail "Required Qt framework is absent: ${expected}"
    done
}

normalize_runtime_rpaths() {
    local bundle="$1"
    local binary
    local rpath
    local safe_rpath

    while IFS= read -r -d '' binary; do
        is_mach_o "${binary}" || continue
        case "${binary}" in
            "${bundle}/Contents/MacOS/"*)
                safe_rpath='@loader_path/../Frameworks'
                ;;
            "${bundle}/Contents/PlugIns/"*)
                safe_rpath='@loader_path/../../Frameworks'
                ;;
            "${bundle}/Contents/Frameworks/"*.framework/Versions/*/*)
                safe_rpath='@loader_path/../../..'
                ;;
            "${bundle}/Contents/Frameworks/"*)
                safe_rpath='@loader_path'
                ;;
            *)
                fail "Mach-O file is outside the approved runtime roots: ${binary}"
                ;;
        esac

        while IFS= read -r rpath; do
            [[ -n "${rpath}" ]] || continue
            install_name_tool -delete_rpath "${rpath}" "${binary}"
        done < <(otool -arch arm64 -l "${binary}" |
            awk '$1 == "cmd" && $2 == "LC_RPATH" { getline; getline; print $2 }')
        install_name_tool -add_rpath "${safe_rpath}" "${binary}"
    done < <(find "${bundle}/Contents" -type f -print0)
}

verify_dependency_policy() {
    local bundle="$1"
    local architecture
    local binary
    local dependency

    while IFS= read -r -d '' binary; do
        is_mach_o "${binary}" || continue
        for architecture in arm64 x86_64; do
            local minimum
            minimum="$(otool -arch "${architecture}" -l "${binary}" |
                awk '$1 == "minos" { print $2 } $1 == "cmd" && $2 == "LC_VERSION_MIN_MACOSX" { getline; getline; print $2 }')"
            [[ -n "${minimum}" ]] || fail "Deployment floor is absent: ${binary} (${architecture})"
            awk -v minimum="${minimum}" 'BEGIN { split(minimum,v,"."); exit !(v[1] < 13 || (v[1] == 13 && v[2] == 0)) }' || \
                fail "Mach-O exceeds macOS 13 floor: ${minimum} (${binary}, ${architecture})"
            while IFS= read -r dependency; do
                dependency="${dependency#"${dependency%%[![:space:]]*}"}"
                dependency="${dependency%% (compatibility version*}"
                [[ -n "${dependency}" ]] || continue
                case "${dependency}" in
                    /System/Library/*|/usr/lib/*)
                        [[ "${dependency}" != *'/../'* ]] || fail "Noncanonical system dependency: ${dependency}"
                        ;;
                    @loader_path/*|@executable_path/*)
                        python3 - "${bundle}" "${binary}" "${dependency}" <<'PY'
import pathlib, sys
bundle, binary, dependency = pathlib.Path(sys.argv[1]).resolve(), pathlib.Path(sys.argv[2]), sys.argv[3]
base = binary.parent if dependency.startswith('@loader_path/') else bundle / 'Contents/MacOS'
resolved = (base / dependency.split('/', 1)[1]).resolve(strict=True)
if not resolved.is_relative_to(bundle):
    raise SystemExit('relative dependency escapes the packaged runtime')
PY
                        ;;
                    @rpath/*)
                        [[ -e "${bundle}/Contents/Frameworks/${dependency#@rpath/}" ]] || \
                            fail "Packaged @rpath dependency is unresolved: ${dependency} (${binary}, ${architecture})"
                        ;;
                    *)
                        fail "External runtime dependency is forbidden: ${dependency} (${binary}, ${architecture})"
                        ;;
                esac
            done < <(otool -arch "${architecture}" -L "${binary}" | tail -n +2)
        done
    done < <(find "${bundle}/Contents" -type f -print0)

    while IFS= read -r -d '' binary; do
        is_mach_o "${binary}" || continue
        for architecture in arm64 x86_64; do
            while IFS= read -r rpath; do
                [[ "${rpath}" == @loader_path* ]] || \
                    fail "Packaged runtime has an untrusted RPATH: ${rpath} (${binary}, ${architecture})"
            done < <(otool -arch "${architecture}" -l "${binary}" |
                awk '$1 == "cmd" && $2 == "LC_RPATH" { getline; getline; print $2 }')
        done
        lipo "${binary}" -verify_arch arm64 x86_64 >/dev/null || \
            fail "Packaged runtime is not universal: ${binary}"
    done < <(find "${bundle}/Contents" -type f -print0)
}

verify_signature_policy() {
    local bundle="$1"
    local details
    local entitlements

    codesign --verify --deep --strict --verbose=4 "${bundle}"
    details="$(codesign -d --verbose=4 "${bundle}" 2>&1)"
    grep -Fq "Identifier=${BUNDLE_ID}" <<<"${details}" || fail "Signed identifier is not ${BUNDLE_ID}"
    grep -Fq "TeamIdentifier=${TEAM_ID}" <<<"${details}" || fail "Signed team is not ${TEAM_ID}"
    grep -Eq 'flags=.*\(runtime\)' <<<"${details}" || fail "Hardened runtime is absent"
    grep -Fq 'Timestamp=' <<<"${details}" || fail "Secure signing timestamp is absent"

    entitlements="$(codesign -d --entitlements :- "${bundle}" 2>/dev/null || true)"
    if grep -q '<key>' <<<"${entitlements}"; then
        fail "Release application unexpectedly carries entitlements"
    fi
}

verify_relocated_launch() {
    local bundle="$1"
    local profile_root="$2"
    local output
    local status

    mkdir -p "${profile_root}"
    if output="$(env -u QT_QPA_PLATFORM -u QT_STYLE_OVERRIDE \
        HOME="${profile_root}" \
        "${bundle}/Contents/MacOS/${APP_NAME}" \
        --self-test --automated-run-budget-ms 8000 2>&1)"; then
        status=0
    else
        status=$?
    fi

    # A release build with a new profile has no configured host, so its
    # composed self-test may return 1 after successfully launching.
    if [[ "${status}" != "0" && "${status}" != "1" ]]; then
        printf '%s\n' "${output}" >&2
        fail "Relocated application did not complete its launch probe (status ${status})"
    fi
    grep -Fq 'AUTOMATED_PROFILE_PRIVATE_OK' <<<"${output}" || \
        fail "Relocated application did not reach the composed launch probe"
    grep -Fq "NOTIFICATION_BUNDLE_IDENTITY:${BUNDLE_ID}" <<<"${output}" || \
        fail "Relocated application did not report the production bundle identity"
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --version)
            [[ $# -ge 2 ]] || fail "--version requires a value"
            REQUESTED_VERSION="${2#v}"
            shift 2
            ;;
        --identity)
            [[ $# -ge 2 ]] || fail "--identity requires a value"
            CODESIGN_IDENTITY="$2"
            shift 2
            ;;
        --notary-profile)
            [[ $# -ge 2 ]] || fail "--notary-profile requires a value"
            NOTARY_PROFILE="$2"
            shift 2
            ;;
        --notary-keychain)
            [[ $# -ge 2 ]] || fail "--notary-keychain requires a value"
            [[ -n "$2" ]] || fail "--notary-keychain requires a nonempty path"
            NOTARY_KEYCHAIN="$2"
            shift 2
            ;;
        --output-dir)
            [[ $# -ge 2 ]] || fail "--output-dir requires a value"
            OUTPUT_DIR="$2"
            shift 2
            ;;
        --skip-tests)
            SKIP_TESTS=1
            shift
            ;;
        --skip-notarize)
            SKIP_NOTARIZE=1
            shift
            ;;
        --prepare-only)
            PREPARE_ONLY=1
            shift
            ;;
        --allow-dirty)
            ALLOW_DIRTY=1
            shift
            ;;
        --variant)
            [[ $# -ge 2 ]] || fail "--variant requires a value"
            VARIANT="$2"
            shift 2
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

[[ "$(uname -s)" == "Darwin" ]] || fail "macOS release packaging requires macOS"
case "${VARIANT}" in
    "" | agent-free | agent-free-automation-off) ;;
    *) fail "--variant must be agent-free or agent-free-automation-off" ;;
esac
PRESET="macos-release${VARIANT:+-${VARIANT}}"
VARIANT_SUFFIX="${VARIANT:+-${VARIANT}}"
BUILD_DIR="${ROOT_DIR}/build/${PRESET}"
APP_PATH="${BUILD_DIR}/src/app/${APP_NAME}.app"
[[ "${REQUESTED_VERSION}" =~ ^[0-9]+\.[0-9]+\.[0-9]+([.-][0-9A-Za-z.-]+)?$ ]] || \
    fail "--version must be a SemVer value"
[[ "$(project_version)" == "${REQUESTED_VERSION}" ]] || \
    fail "Requested version ${REQUESTED_VERSION} does not match CMake project version $(project_version)"

if [[ "${ALLOW_DIRTY}" == "0" && -n "$(git -C "${ROOT_DIR}" status --porcelain)" ]]; then
    fail "Working tree has uncommitted changes. Commit them or pass --allow-dirty."
fi

require_command cmake
require_command codesign
require_command ditto
require_command env
require_command file
require_command hdiutil
require_command install_name_tool
require_command lipo
require_command otool
require_command shasum
require_command python3

if [[ "${PREPARE_ONLY}" == "0" ]]; then
    require_command security
    require_command xcrun
    security find-identity -v -p codesigning 2>/dev/null |
        grep -Fq "\"${CODESIGN_IDENTITY}\"" || fail "Codesigning identity is unavailable: ${CODESIGN_IDENTITY}"
fi

if [[ "${PREPARE_ONLY}" == "0" && "${SKIP_NOTARIZE}" == "0" ]]; then
    require_command spctl
    [[ -n "${NOTARY_PROFILE}" ]] || fail "A notarytool Keychain profile is required"
    if [[ -n "${NOTARY_KEYCHAIN}" ]]; then
        [[ -f "${NOTARY_KEYCHAIN}" ]] || fail "Notary Keychain is absent: ${NOTARY_KEYCHAIN}"
    fi
fi

if [[ "${SKIP_TESTS}" == "0" ]]; then
    log "Building and testing the development graph"
    cmake --preset dev
    cmake --build --preset dev
    python3 scripts/run_test_suite.py --preset dev
else
    log "Skipping tests"
fi

log "Building the universal tests-disabled application (${PRESET})"
cmake --preset "${PRESET}"
cmake --build --preset "${PRESET}"
[[ -d "${APP_PATH}" ]] || fail "Expected application bundle is absent: ${APP_PATH}"

INFO_PLIST="${APP_PATH}/Contents/Info.plist"
[[ "$(plist_value CFBundleIdentifier "${INFO_PLIST}")" == "${BUNDLE_ID}" ]] || \
    fail "Built bundle identifier is not ${BUNDLE_ID}"
[[ "$(plist_value CFBundleShortVersionString "${INFO_PLIST}")" == "${REQUESTED_VERSION}" ]] || \
    fail "Built bundle version does not match ${REQUESTED_VERSION}"

MACDEPLOYQT="${MACDEPLOYQT:-}"
if [[ -z "${MACDEPLOYQT}" ]]; then
    QT6_DIR="$(sed -n 's/^Qt6_DIR:PATH=//p' "${BUILD_DIR}/CMakeCache.txt" | head -n 1)"
    [[ -n "${QT6_DIR}" ]] || fail "Qt6_DIR is absent from the release CMake cache"
    MACDEPLOYQT="$(cd "${QT6_DIR}/../../../bin" && pwd)/macdeployqt"
fi
[[ -x "${MACDEPLOYQT}" ]] || fail "macdeployqt is unavailable: ${MACDEPLOYQT}"

if [[ -z "${OUTPUT_DIR}" ]]; then
    OUTPUT_DIR="${ROOT_DIR}/build/releases/${REQUESTED_VERSION}"
elif [[ "${OUTPUT_DIR}" != /* ]]; then
    OUTPUT_DIR="${ROOT_DIR}/${OUTPUT_DIR}"
fi
[[ "${OUTPUT_DIR}" != "/" && "${OUTPUT_DIR}" != "${ROOT_DIR}" ]] || \
    fail "Release output directory is too broad: ${OUTPUT_DIR}"
mkdir -p "${OUTPUT_DIR}"
STAGING_ROOT="$(mktemp -d "${OUTPUT_DIR}/.staging.XXXXXX")"
trap 'rm -rf "${STAGING_ROOT}"' EXIT
STAGED_APP="${STAGING_ROOT}/${APP_NAME}.app"

log "Deploying the required Qt runtime"
ditto "${APP_PATH}" "${STAGED_APP}"
"${MACDEPLOYQT}" "${STAGED_APP}" -always-overwrite -codesign=- -verbose=1
verify_plugin_policy "${STAGED_APP}"
verify_framework_policy "${STAGED_APP}"
normalize_runtime_rpaths "${STAGED_APP}"
cat >"${STAGED_APP}/Contents/Resources/qt.conf" <<'EOF'
[Paths]
Plugins = PlugIns
EOF
verify_dependency_policy "${STAGED_APP}"
log "Preparing exact third-party notices and corresponding source"
SOURCE_ASSET="Source-${APP_NAME}-${REQUESTED_VERSION}-macos-universal-corresponding-source.zip"
SOURCE_PATH="${OUTPUT_DIR}/${SOURCE_ASSET}"
python3 "${ROOT_DIR}/packaging/package_runtime.py" notices --build "${BUILD_DIR}" --root "${STAGED_APP}" --source-archive "${SOURCE_PATH}"

if [[ "${PREPARE_ONLY}" == "1" ]]; then
    ASSET_BASE="${APP_NAME}-${REQUESTED_VERSION}-macos-universal${VARIANT_SUFFIX}-local"
    LOCAL_APP="${OUTPUT_DIR}/${APP_NAME}.app"
    LOCAL_ZIP="${OUTPUT_DIR}/${ASSET_BASE}.zip"
    [[ ! -e "${LOCAL_APP}" && ! -e "${LOCAL_ZIP}" ]] || fail "Use a fresh output directory for local preparation"
    log "Sealing the local candidate with an offline ad-hoc signature"
    codesign --force --deep --sign - --timestamp=none "${STAGED_APP}"
    codesign --verify --deep --strict "${STAGED_APP}"
    verify_relocated_launch "${STAGED_APP}" "${STAGING_ROOT}/smoke-profile"
    ditto "${STAGED_APP}" "${LOCAL_APP}"
    python3 "${ROOT_DIR}/packaging/package_runtime.py" audit --build "${BUILD_DIR}" --root "${LOCAL_APP}" --output "${OUTPUT_DIR}/${ASSET_BASE}.spdx.json" --source-archive "${SOURCE_PATH}"
    ditto -c -k --keepParent "${LOCAL_APP}" "${LOCAL_ZIP}"
    (cd "${OUTPUT_DIR}" && shasum -a 256 "${ASSET_BASE}.zip" "${ASSET_BASE}.spdx.json" "${SOURCE_ASSET}" > "${ASSET_BASE}.sha256")
    log "Local candidate: ${LOCAL_APP}; not Developer-ID signed, notarized, Gatekeeper-qualified or published"
    exit 0
fi

log "Signing nested code and application with Developer ID"
sign_nested_code "${STAGED_APP}"
verify_signature_policy "${STAGED_APP}"
verify_relocated_launch "${STAGED_APP}" "${STAGING_ROOT}/smoke-profile"

ASSET_BASE="${APP_NAME}-${REQUESTED_VERSION}-macos-universal${VARIANT_SUFFIX}"
DMG_PATH="${OUTPUT_DIR}/${ASSET_BASE}.dmg"
ZIP_PATH="${OUTPUT_DIR}/${ASSET_BASE}.zip"
DMG_CHECKSUM_PATH="${DMG_PATH}.sha256"
ZIP_CHECKSUM_PATH="${ZIP_PATH}.sha256"
NOTARY_ZIP="${STAGING_ROOT}/${ASSET_BASE}-notary.zip"

rm -f "${DMG_PATH}" "${ZIP_PATH}" "${DMG_CHECKSUM_PATH}" "${ZIP_CHECKSUM_PATH}"

if [[ "${SKIP_NOTARIZE}" == "0" ]]; then
    log "Submitting the signed application to Apple"
    ditto -c -k --keepParent "${STAGED_APP}" "${NOTARY_ZIP}"
    notary_submit "${NOTARY_ZIP}"
    xcrun stapler staple "${STAGED_APP}"
    xcrun stapler validate "${STAGED_APP}"
    spctl --assess --type execute --verbose=4 "${STAGED_APP}"
else
    log "Skipping notarization and Gatekeeper assessment"
fi

DMG_ROOT="${STAGING_ROOT}/dmg-root"
mkdir -p "${DMG_ROOT}"
ditto "${STAGED_APP}" "${DMG_ROOT}/${APP_NAME}.app"
ln -s /Applications "${DMG_ROOT}/Applications"

log "Creating and signing the release DMG"
hdiutil create -volname "${APP_NAME}" -srcfolder "${DMG_ROOT}" -ov -format UDZO "${DMG_PATH}"
codesign --force --timestamp --sign "${CODESIGN_IDENTITY}" "${DMG_PATH}"
codesign --verify --strict --verbose=4 "${DMG_PATH}"

if [[ "${SKIP_NOTARIZE}" == "0" ]]; then
    log "Submitting and stapling the release DMG"
    notary_submit "${DMG_PATH}"
    xcrun stapler staple "${DMG_PATH}"
    xcrun stapler validate "${DMG_PATH}"
    spctl --assess --type open --context context:primary-signature --verbose=4 "${DMG_PATH}"
fi

log "Creating the release ZIP and checksums"
ditto -c -k --keepParent "${STAGED_APP}" "${ZIP_PATH}"
python3 "${ROOT_DIR}/packaging/package_runtime.py" audit --build "${BUILD_DIR}" --root "${STAGED_APP}" --output "${OUTPUT_DIR}/${ASSET_BASE}.spdx.json" --source-archive "${SOURCE_PATH}"
(
    cd "${OUTPUT_DIR}"
    shasum -a 256 "$(basename "${DMG_PATH}")" >"$(basename "${DMG_CHECKSUM_PATH}")"
    shasum -a 256 "$(basename "${ZIP_PATH}")" >"$(basename "${ZIP_CHECKSUM_PATH}")"
    shasum -a 256 "${ASSET_BASE}.spdx.json" >"${ASSET_BASE}.spdx.json.sha256"
    shasum -a 256 "${SOURCE_ASSET}" >"${SOURCE_ASSET}.sha256"
)

cat <<EOF

Release package complete
  Version: ${REQUESTED_VERSION}
  Bundle:  ${BUNDLE_ID}
  Team:    ${TEAM_ID}
  DMG:     ${DMG_PATH}
  ZIP:     ${ZIP_PATH}
  Source:  ${SOURCE_PATH}
  SHA-256: ${DMG_CHECKSUM_PATH}
           ${ZIP_CHECKSUM_PATH}
EOF
