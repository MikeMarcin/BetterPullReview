# Linux release

Use x86_64 Ubuntu 26.04 LTS for DEB and the per-user archive. The owner's
2026-10-11 next-release amendment removes Ubuntu 24.04 and RHEL from the
selected Linux release set. WSL2 is an admitted producer for ordinary Linux
binaries; WSL/WSLg observations do not establish clean Ubuntu desktop, vault
or installer qualification. The Ubuntu closure enforces a glibc 2.43 ceiling; retain
actual GLIBCXX/system dependency constraints and private Qt/XCB runtime.
Configure success does not qualify a compiler/libstdc++ baseline.

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
  --gpg-key APPROVED_FULL_FINGERPRINT
```

The driver detach-signs final installers, per-user archives, SPDX, source ZIP,
checksums and provenance. It verifies the approved primary/signing fingerprint
for every detached signature. RPM is also signed with `rpmsign` before final
hashing and checked with `rpmkeys` against only that approved public key in a
disposable verification database; the host's trust database is unchanged.
Obtain the approved public verification key/fingerprint through the intended
channel. No private key or passphrase is placed on a command line by the driver;
use the owner's secure GPG agent/pinentry setup.

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
material. No Linux key is chosen as of the 2026-10-11 preparation; signing remains
pending. A future release request must select its version before production.

The existing RHEL 10 RPM mechanics are retained as optional compatibility, with
their glibc 2.39 ceiling and x86-64-v3 CPU floor. They are outside the selected
release set and do not require provisioning a RHEL producer. If separately
selected later, use its native baseline and signing/qualification gates.

Primary references: [GnuPG commands](https://gnupg.org/documentation/manuals/gnupg/Operational-GPG-Commands.html)
and [RPM signing](https://rpm.org/docs/4.20.x/man/rpmsign.8).
