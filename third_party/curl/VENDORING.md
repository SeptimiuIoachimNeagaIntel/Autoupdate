# Vendored curl (pruned)

**Version:** 8.10.1
**Upstream:** https://curl.se/download/curl-8.10.1.tar.gz

This is **not** a pristine upstream tree. Do not verify it against the upstream
tarball checksum directly. It keeps only the libcurl sources and the CMake
build system: the full upstream tarball is ~30 MB, this tree is ~6 MB.

Unlike the OpenSSL tree there is no `scripts/vendor-curl.sh`; to bump the
version, extract the upstream tarball over this directory and re-apply the
removals listed below.

## What is kept

- `CMakeLists.txt`, `CMake/` — the CMake build system, kept whole. The
  `Find*.cmake` modules for TLS backends this project does not use are tiny and
  are kept so that switching backends stays a one-line change.
- `lib/` — the complete libcurl source tree, including `lib/Makefile.inc`,
  which `lib/CMakeLists.txt` parses to get the source list, and
  `lib/curl_config.h.cmake`, the CMake config header template.
- `include/curl/*.h` — the public headers.
- `COPYING` — the license.

`lib/` is kept complete on purpose. Every `.c` file listed in
`lib/Makefile.inc` is compiled unconditionally and guarded by internal
`#ifdef`s, so the unused TLS/SSH/QUIC backends in `lib/vtls/`, `lib/vssh/` and
`lib/vquic/` cannot be removed without editing `Makefile.inc`. The
`lib/config-*.h` and `lib/setup-*.h` per-platform headers are kept for the same
reason: they are part of the library source, and the gain from removing them is
a few tens of kilobytes.

## What was removed

Other build systems — this project builds curl only through
`add_subdirectory(third_party/curl)`:

- `Makefile.am`, `Makefile.in`, `Makefile.dist`
- `configure`, `configure.ac`, `acinclude.m4`, `aclocal.m4`, `m4/`
- `compile`, `config.guess`, `config.sub`, `depcomp`, `install-sh`,
  `ltmain.sh`, `missing` (autotools helper scripts)
- `lib/Makefile.am`, `lib/Makefile.in`, `lib/Makefile.mk`,
  `lib/Makefile.soname`, `lib/curl_config.h.in`, `lib/libcurl.vers.in`
- `include/Makefile.am`, `include/Makefile.in`, `include/curl/Makefile.am`,
  `include/curl/Makefile.in`
- `winbuild/` (nmake), `projects/` (Visual Studio project generator),
  `plan9/`, `packages/` (OS/400 and VMS), `buildconf.bat`

Install-time artifacts — the parent `CMakeLists.txt` forces
`CURL_DISABLE_INSTALL=ON`, which skips the whole `install()` block that reads
these:

- `curl-config.in`, `libcurl.pc.in`

Everything else:

- `tests/` (12 MB test suite; `BUILD_TESTING` and `CURL_DISABLE_TESTS` are
  forced so it is never configured)
- `docs/` (5.5 MB: man page sources, `docs/examples/`, `docs/internals/`)
- `src/` (the `curl` command-line tool; `BUILD_CURL_EXE` is forced off)
- `scripts/` (release, lint and documentation-generation tooling)
- `lib/optiontable.pl` (regenerates `easyoptions.c`; a maintainer tool)
- `CHANGES.md`, `RELEASE-NOTES`, `README`, `Dockerfile`

## Required parent-project options

Removing `docs/` means curl's own `CMakeLists.txt` must be told not to descend
into it. `HAVE_MANUAL_TOOLS` is set whenever Perl is found, and it gates
`add_subdirectory(docs)`, so the parent `CMakeLists.txt` forces:

    BUILD_LIBCURL_DOCS=OFF
    BUILD_MISC_DOCS=OFF
    ENABLE_CURL_MANUAL=OFF
    BUILD_EXAMPLES=OFF
    BUILD_CURL_EXE=OFF
    BUILD_TESTING=OFF
    CURL_DISABLE_TESTS=ON
    CURL_DISABLE_INSTALL=ON

If any of those is turned back on, the matching directory has to be restored
from the upstream tarball first. Turning the manuals off also means the
Windows and macOS builds no longer need Perl at all.

## Verification

Configuring and building before and after the prune yields the same
`-- Protocols:`, `-- Features:` and `-- Enabled SSL backends:` summary lines
and the same `jlp_downloader.exe` size, so no compiled source was lost.
