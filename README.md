# JLP Tool Downloader

Finds and downloads the highest-versioned `JLP_<version>.zip` archive from the Artifactory
folder:

```
https://af01p-ir.devtools.intel.com/artifactory/mvt-releases-local/JLPTool
```

It lists the folder with one Artifactory REST storage API request, compares
the numeric version components in each matching filename, and downloads the
archive with the highest version using libcurl.

## Dependencies vendored in this repo (no network access needed to build)

- `third_party/curl` — libcurl 8.10.1 source, built from source as a static
  library via `add_subdirectory`. This is a **pruned** tree (~6 MB rather than
  the full ~30 MB): it keeps the library sources and the CMake build, and drops
  the test suite, the docs, the `curl` command-line tool and the autotools and
  nmake build systems. See `third_party/curl/VENDORING.md`.
- `third_party/openssl` — OpenSSL 3.5.8 (LTS) source, required for offline
  Linux HTTPS support and built locally by CMake. Also a **pruned** tree
  (~35 MB rather than the full ~140 MB); see
  `third_party/openssl/VENDORING.md` for what was removed and how to
  regenerate or bump it via `scripts/vendor-openssl.sh`.
- `third_party/json` — nlohmann/json v3.11.3 single-header library.

Both pruned trees keep everything the build actually compiles or reads,
including sources for platforms this project does not currently target, so
neither prune narrows the set of platforms that can be built.

All three are committed to the repository, so building this project does not
require internet access or a package manager.

## Building

CMake auto-detects the compiler (Visual Studio on Windows, GCC/Clang on
Linux) — no toolchain is specified explicitly.

```powershell
cmake -S . -B build
cmake --build build --config Release
```

On Windows, TLS is provided by the native Schannel backend. On macOS, TLS is
provided by the native Secure Transport backend. On Linux, CMake builds the
committed OpenSSL source from `third_party/openssl` into the build directory
and points libcurl at that local copy. The Linux build does not download
anything and does not use a system OpenSSL package.

The Linux OpenSSL build runs during `cmake -S . -B build`, not during
`cmake --build`, because libcurl's `find_package(OpenSSL REQUIRED)` and its
feature probes need the real libraries at configure time. The first configure
therefore takes a few minutes; afterwards a stamp file in the build directory
short-circuits it, so later configures are fast. Deleting `build/` restarts
that one-time cost. Building OpenSSL also requires `perl` and `make` on
`PATH` — both standard on Linux.

Windows and macOS builds need neither: the only thing that used to want Perl
there was curl's man page generation, which is switched off because the
vendored curl tree has no `docs/` directory.

## Running

```powershell
./build/Release/jlp_downloader.exe [folderUrl] [outputDir]
```

Both arguments are optional:
- `folderUrl` defaults to the JLPTool Artifactory folder above.
- `outputDir` defaults to the current directory.
