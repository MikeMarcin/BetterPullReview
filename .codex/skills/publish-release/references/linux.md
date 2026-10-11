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
Without signing options it remains unsigned and emits no release manifest. It never publishes.

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

Use the integrated signing path on the matching producer:

```sh
python3 packaging/package.py --build build/linux-package \
  --generator DEB --output build/releases/X.Y.Z/ubuntu-24.04-x64 \
  --gpg-key APPROVED_FULL_FINGERPRINT
# On RHEL 10, use --generator RPM and a separate output directory.
```

The driver detach-signs final installers, per-user archives, SPDX, source ZIP,
checksums and provenance. It verifies the approved primary/signing fingerprint
for every detached signature. RPM is also signed with `rpmsign` before final
hashing and checked with `rpmkeys` against only that approved public key in a
disposable verification database; the host's trust database is unchanged.
Obtain the approved public verification key/fingerprint through the intended
channel. No private key or passphrase is placed on a command line by the driver;
use the owner's secure GPG agent/pinentry setup.

A successful signed run emits `*.deb.release.json` or `*.rpm.release.json`.
Names include `linux-ubuntu-24.04-x64` or `linux-rhel-10-x64`, including installed
source URLs, so both baselines coexist. Reconfigure older distribution builds;
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
Keep Ubuntu/RHEL installation/authentication/Secret Service vault, purge/residuals
and removal qualification explicit. Containers can prove command behavior but
not clean desktop qualification. Return to shared publication with both manifests
when a complete Linux set is requested.

Primary references: [GnuPG commands](https://gnupg.org/documentation/manuals/gnupg/Operational-GPG-Commands.html)
and [RPM signing](https://rpm.org/docs/4.20.x/man/rpmsign.8).
