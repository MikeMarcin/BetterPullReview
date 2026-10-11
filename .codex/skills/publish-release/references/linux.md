# Linux release

Use native x86_64 Ubuntu 24.04 LTS for DEB and RHEL 10 for RPM. RHEL requires
x86-64-v3. Do not substitute Ubuntu 26.04/WSL, cross-builds or another distro for
declared baseline evidence. Both closures enforce a glibc 2.39 ceiling; retain
actual GLIBCXX/system dependency constraints and private Qt/XCB runtime.
Configure success does not qualify a compiler/libstdc++ baseline.

Require the package guide's native C++23 toolchain, Python 3.10+, Qt producer
SPDX, `patchelf`, `readelf`, `ldd`, libsecret/libxcb development files and
native packaging tools. Run the normal native test gate, then:

```sh
cmake --preset linux-package
python3 packaging/package.py --build build/linux-package \
  --generator DEB --output build/releases/X.Y.Z/ubuntu-24.04-x64
```

On RHEL use `--generator RPM` and separate
`build/releases/X.Y.Z/rhel-10-x64` output. The driver requires new directories,
builds selected app/helpers, runs CPack and emits installer, SPDX, separate source
ZIP, checksum, provenance and `*-user.tar.gz` from the audited runtime.
It neither signs nor publishes.

DEB/RPM are managed system installs; per-user archives require system dependencies
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

For direct private GitHub distribution, detach-sign final DEB and per-user
archives, plus source/SPDX/final checksum metadata selected in the manifest:

```sh
gpg --local-user APPROVED_FULL_FINGERPRINT --armor --detach-sign \
  --output /absolute/artifact.deb.asc /absolute/artifact.deb
gpg --verify /absolute/artifact.deb.asc /absolute/artifact.deb
```

Require success and the approved signing fingerprint; an arbitrary key's
"Good signature" is insufficient. Deliver public verification material/fingerprint
through the approved channel. Detached signatures do not make `dpkg` enforce
publisher trust: recipients verify before installation. A signed corporate APT
repository is a separately selected route, not created by this skill.

For RPM, use `rpmsign --addsign /absolute/artifact.rpm` with explicitly approved
key configuration. Require `rpmkeys --checksig /absolute/artifact.rpm` against
that key; stop on NOKEY, absent/bad signatures, wrong signer or digest failure.
Sign RPM before final checksums/provenance or detached signatures.

RPM signing changes package bytes. Finalize affected checksum entries/provenance
through reviewed existing tooling while retaining source/SPDX/archive hashes,
producer identity and signing evidence. `package.py` has no signed RPM finalizer;
do not advertise a pre-sign package hash or flip a boolean as proof. Detached
DEB/archive signatures preserve original digests, but the final manifest must
also enumerate/hash signatures and distinguish unsigned producer evidence from
final release trust.

## Coordinate assets and publication

The set contains each selected installer, per-user archive, SPDX, separate source
ZIP, final checksum/provenance and signatures. Prepare both baselines for a
requested complete Linux set. Native names use `linux-x64` without a distro
identifier: archives, SPDX or source names may collide. Compare names/bytes before
upload. Never overwrite one baseline or rename source assets after references
were embedded. Distinct assets need baseline-qualified naming in the existing
producer before rebuilding; deduplicate only byte-identical assets whose
installed bindings remain correct.

Return to shared publication: the current macOS publisher cannot upload this
set. Keep Ubuntu/RHEL installation/authentication/Secret Service vault,
purge/residuals/removal qualification explicit. Signatures do not complete
RFC-BPR-0038's native/security criteria.

Primary references: [GnuPG commands](https://gnupg.org/documentation/manuals/gnupg/Operational-GPG-Commands.html)
and [RPM signing](https://rpm.org/docs/4.20.x/man/rpmsign.8).
