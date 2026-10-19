# Rebuild the shipped Linux ICU 73.2 companions

The source asset includes `ICU-73.2.tar.gz`, the unmodified upstream
`icu4c-73_2-src.tgz`, plus this directory and
`ICU-73.2-build/packaging/linux_icu.py`. Keep their relative source-asset layout.
The complete mixed ICU/data/dictionary/build-tool notice is `icu/LICENSE` in
the archive and is also retained in the installed notices. No single aggregate
licence conclusion is asserted.

Use native Ubuntu26.04 x86_64, Python3.12 or newer, GNU make and a working
C/C++ toolchain. The producer uses GCC15.2.0; the installed
`ThirdParty/icu-runtime-provenance.json` records its exact source, recipe,
compiler/make identities, flags, commands and output hashes. Source-built ICU
is distinct from Qt's prebuilt ABI inputs. No signing key is needed to rebuild.

For an unmodified rebuild, run from the extracted source asset directory with
your own absolute archive, helper, build and compiler paths:

```sh
python3 /absolute/ICU-73.2-build/packaging/linux_icu.py \
  --archive /absolute/ICU-73.2.tar.gz \
  --sha256 818a80712ed3caacd9b652305e01afc7fa167e6f2e94996da44b90c2ab604ce1 \
  --build /absolute/new-icu-build \
  --cc /usr/bin/cc --cxx /usr/bin/c++
```

The helper verifies the archive before creating that fresh root, uses an
out-of-source build, disables tests/samples/extras/icuio, and runs no tests or
application. It preserves PIC, stack protection, frame pointers and
`_FORTIFY_SOURCE=3`, and links full RELRO/NOW with a nonexecutable stack.
Only the `data`, `uc` and `i18n` shared libraries are distributed; other build
helpers stay in the private build root. A cache is reusable only when source,
recipe, compiler/make, flags, extracted source and all three outputs still match.
A failed partial root is preserved; select another fresh path to retry.

The files under `prefix/lib` are `libicudata.so.73.2`, `libicuuc.so.73.2` and
`libicui18n.so.73.2`. To replace the package's corresponding `.so.73` files,
copy these three files under those SONAME filenames, then apply the producer's
relative loader path with `patchelf --set-rpath '$ORIGIN'` to each copied file.
Keep all three together beside the app and private Qt libraries. Do not alter a
system ICU or the Qt SDK. Verify `readelf -W -l -d` reports GNU_RELRO, NOW and
an RW (nonexecutable) GNU_STACK, and that `ldd` resolves this private trio.

For a modified compatible build, edit a copy of the upstream source and repack
it, pass its new SHA-256 to the same helper and use a fresh build directory.
Preserve SONAME73 and the ICU entry points QtCore imports. Modified bytes are
not the producer's signed final artifacts. The existing source-access,
redistribution and retention arrangement applies to the original source asset;
these instructions supply practical build/replacement material, not release
security or legal acceptance.
