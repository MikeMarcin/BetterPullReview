# Linux release

Use x86_64 Ubuntu 26.04 LTS for DEB and the per-user archive. The owner's
2026-10-11 next-release amendment removes Ubuntu 24.04 and RHEL from the
selected Linux release set. WSL2 is an admitted producer for ordinary Linux
binaries; WSL/WSLg observations do not establish clean Ubuntu desktop, vault
or installer qualification. The Ubuntu closure enforces a glibc 2.43 ceiling; retain
actual GLIBCXX/system dependency constraints and private Qt/XCB runtime.
Configure success does not qualify a compiler/libstdc++ baseline.

Use the producer's configured native shell/tool PATH when dispatching from
Windows or the Mac host. A bare `wsl --exec` may omit user-installed CMake/Ninja
even when the station's login shell finds them; resolve tools through that
configured environment and pass absolute source/build/output paths. The
2026-10-11 focused course used `wsl --exec bash -lc` and required no installation.

For Mac-to-Windows SSH dispatch, start PowerShell in an accessible Windows
directory (`Set-Location C:/`) before invoking WSL. The 2026-10-16 preparation
observed `Wsl/ERROR_PATH_NOT_FOUND` with a direct SSH launch; the explicit
Windows directory and `wsl.exe -d Ubuntu --exec bash -l ABSOLUTE_SCRIPT` worked.
Put a multi-command course in that script rather than nesting shell quotations
across SSH, PowerShell and Bash. Append the producer's approved local patchelf
directory when it is absent from the login PATH; do not install or replace tools
merely because a bare dispatch cannot find them.

Unversioned readiness may check the baseline, toolchain, public-key digest and
focused packaging contracts without producing a release. Native Windows
`release_gpg_vault.py validate --config ABSOLUTE_CONFIG` checks the scoped key,
protection and agent configuration without retrieving the credential, unlocking
or signing. Its success does not qualify credential retrieval from that remote
logon. Preserve the existing successful enrollment; no passphrase prompt is
needed for these checks. The current
[readiness record](../../../../docs/verification/ubuntu-release-readiness-20261016.md)
binds the observed producer and remaining release requirements.

Require the package guide's native C++23 toolchain, Python 3.10+, Qt producer
SPDX, `patchelf`, `readelf`, `ldd`, libsecret/libxcb development files and
native packaging tools. Run the normal native test gate, then:

```sh
cmake -S /absolute/source -B /absolute/source/build/linux-package --preset linux-package
python3 /absolute/source/packaging/package.py --build /absolute/source/build/linux-package \
  --generator DEB --output /absolute/source/build/releases/X.Y.Z/ubuntu-26.04-x64
```

Use the exact committed source revision and selected version shared by all
producers. CPack defaults to DEB on Ubuntu. The driver requires new directories,
builds selected app/helpers, runs CPack and emits installer, SPDX, separate source
ZIP, checksum, provenance and `*-user.tar.gz` from the audited runtime.
Without signing options it remains unsigned and emits no release manifest. It never publishes.

DEB is a managed system install; per-user archives require system dependencies
already installed. Do not introduce global Qt/library paths or bundle undeclared
system libraries. Audit actual expanded installer/archive runtimes, notices,
source binding, package dependencies, ELF hardening and loader origins with the
existing native package/security tools.

## Sign and verify final bytes

Linux signing is independent of Azure Authenticode. Obtain the owner's approved
OpenPGP key and full fingerprint through the intended distribution channel;
the Azure setup establishes no Linux key. Do not silently generate/import/trust
a release key. Keep private keys/passphrases out of source, arguments, logs and
assets; use the owner's secure signing mechanism.

Use the integrated signing path on the matching producer:

```sh
python3 /absolute/source/packaging/package.py --build /absolute/source/build/linux-package \
  --generator DEB --output /absolute/source/build/releases/X.Y.Z/ubuntu-26.04-x64 \
  --gpg-vault-config /absolute/producer-local/ubuntu26-gpg-vault.json
```

The driver detach-signs final installers, per-user archives, SPDX, source ZIP,
checksums and provenance. It verifies the approved primary/signing fingerprint
for every detached signature. RPM is also signed with `rpmsign` before final
hashing and checked with `rpmkeys` against only that approved public key in a
disposable verification database; the host's trust database is unchanged.
Obtain the approved public verification key/fingerprint through the intended
channel. No private key or passphrase is placed on a command line by the driver;
use the owner's secure GPG agent/pinentry setup for producers without the
configured Windows vault route (`--gpg-key APPROVED_FULL_FINGERPRINT`).

### Reusable Windows credential on the WSL producer

The owner approved an Ed25519 release key on 2026-10-11, UID
`BetterPullReview Release Signing`, expiring 2028-10-11. The producer's
nonsecret prerequisite record holds its approved full fingerprint/public-key
digest and scoped GPG home. Its local signing/public-only verification/tamper
smoke passed; no Linux release package has been signed or qualified. Keep the
exact fingerprint in approved producer configuration and obtain the public key
through the intended distribution channel.
When preparing a release, supply that approved exported public
verification key and full fingerprint through the same approved release channel,
alongside the final signatures, checksums, SPDX and corresponding source. Match
the producer record's public-key digest before handing it over. Keep all existing
asset/source/manifest guards; private keys and passphrases stay on the producer.

`packaging/release_gpg_vault.py` enrolls one named generic Windows credential,
`BetterPullReview/ReleaseSigning/OpenPGP/FULL_FINGERPRINT`, user
`BetterPullReviewReleaseSigning`, with local-machine persistence for the same
Windows user. It neither enumerates the vault nor offers secret output.
The private OpenPGP key stays encrypted. The approved producer-local JSON has
this schema; replace every placeholder with the existing approved values:

```json
{
  "schema": "bpr.release-gpg-vault.v1",
  "approved": true,
  "keyFingerprint": "APPROVED_FULL_UPPERCASE_FINGERPRINT",
  "keygrip": "APPROVED_40_CHARACTER_UPPERCASE_KEYGRIP",
  "gnupgHome": "/home/producer/.local/share/bpr-release-signing/gnupg",
  "wslDistribution": "Ubuntu",
  "wslUser": "producer",
  "windowsPython": "C:/absolute/Python/python.exe"
}
```

The GPG home and its parent must be canonical, owned by the ordinary WSL
signing user and mode 0700, with a private agent configuration. Validation
requires the exact usable primary signing fingerprint/keygrip/UID, no signing
subkey, and agent `KEYINFO` passphrase protection. Agent configuration retains
`default-cache-ttl 600`, `max-cache-ttl 7200` and `no-allow-external-cache`.
`configure-agent` adds `allow-preset-passphrase` only there and reloads only that
agent; it refuses other options. Preset cache lifetime remains bounded by the
agent's maximum TTL. [GnuPG's preset documentation](https://www.gnupg.org/documentation/manuals/gnupg/Invoking-gpg_002dpreset_002dpassphrase.html)
defines its standard-input and agent-option contract.

One-time setup on the signing Windows account, using absolute paths:

```powershell
& 'C:/absolute/Python/python.exe' 'C:/absolute/source/packaging/release_gpg_vault.py' configure-agent --config 'C:/absolute/producer-local/ubuntu26-gpg-vault.json'
& 'C:/absolute/Python/python.exe' 'C:/absolute/source/packaging/release_gpg_vault.py' enroll --config 'C:/absolute/producer-local/ubuntu26-gpg-vault.json'
```

Enrollment must run in a visible console for direct human entry, with two
hidden prompts; a wizard hands over that action. Re-enrollment is unnecessary
for future releases. Use `enroll --replace` only for an intended credential
rotation. [Windows generic credential persistence](https://learn.microsoft.com/en-us/windows/win32/api/wincred/ns-wincred-credentialw)
belongs to that user on that computer; it is not an account-independent secret.

Future WSL package commands use the vault config's absolute path (for example
its `/mnt/c/...` path). The helper validates the bound destination before one
`CredReadW`, converts its native UTF-16LE blob to UTF-8 in memory and pipes it to
the fixed scoped `gpg-preset-passphrase` standard input. An in-memory signing
probe with `--pinentry-mode error` confirms the passphrase works. Wrong or
missing credentials fail closed; a wrong passphrase is forgotten from the
agent cache. Every package signature uses the same scoped home with pinentry
disabled. No secret argument, environment variable, file or log is used; mutable
buffers are cleared, without claiming Python can erase every interpreter copy.

Local-account Windows/WSL interop is required. Network-only Windows logons may
lack a credential set; do not fall back to an interactive prompt or claim remote
station qualification from local success. Azure authentication remains separate.
Run `tests/scripts/release_gpg_vault_course.py --config ABSOLUTE_CONFIG` on the
Windows producer for a disposable encrypted-key/credential course; it does not
access the approved credential. Real enrollment/cold-agent qualification is
recorded separately by the coordinator. On 2026-10-11 the owner completed
direct enrollment, and the approved key passed two separate cold-agent unlock,
pinentry-disabled sign/public-only verify/tamper courses. The first visible
terminal could not see the agent-created public config; placing that same
public binding in the actual native producer context resolved it before any
secret prompt. Validate config existence in the process that will enroll/sign,
not solely through an agent filesystem observation.

A successful selected run emits `*.deb.release.json`.
Names include `linux-ubuntu-26.04-x64`, including installed
source URLs. Reconfigure older distribution builds;
do not rename already-produced source assets. Identical source assets across
variants can be deduplicated only when names and bytes match.

Before publication and after download, run `gpg --verify SIGNATURE ARTIFACT` for
every declared signature and match the approved full fingerprint. Also run
`rpmkeys --checksig --verbose PACKAGE` against the approved key for RPM; stop on
NOKEY, wrong signer or bad signature/digest. Detached DEB signatures do not make
`dpkg` enforce publisher trust: recipients verify before installation. A signed
APT repository remains a separately selected route.

Audit actual expanded installer/archive runtimes with the native package/security
tools. The manifest preflight checks the archive's complete SPDX inventory and
source binding, final hashes and signing receipts; offline receipts are not
independent native trust or proof that installer extraction matches the archive.
Keep Ubuntu installation/authentication/Secret Service vault, purge/residuals
and removal qualification explicit. Containers can prove command behavior but
not clean desktop qualification. For a coordinated Mac-host publication, return
the complete final output directory and DEB manifest to the Mac publisher after
the native producer's trust checks. Keep private keys and credentials on the
approved signing PC; transfer only intended final assets and public verification
material. The approved key and reusable vault mechanism are producer
prerequisites; exact final package trust and clean native qualification remain
pending. A future release request must select its version before production.

The existing RHEL 10 RPM mechanics are retained as optional compatibility, with
their glibc 2.39 ceiling and x86-64-v3 CPU floor. They are outside the selected
release set and do not require provisioning a RHEL producer. If separately
selected later, use its native baseline and signing/qualification gates.

Primary references: [GnuPG commands](https://gnupg.org/documentation/manuals/gnupg/Operational-GPG-Commands.html)
and [RPM signing](https://rpm.org/docs/4.20.x/man/rpmsign.8).
