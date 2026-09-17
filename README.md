# JLP Tool Downloader

Finds and downloads the newest `JLP*.zip` archive from the Artifactory
folder:

```
https://af01p-ir.devtools.intel.com/artifactory/mvt-releases-local/JLPTool
```

It lists the folder via the Artifactory REST storage API, reads each
matching file's `lastModified` timestamp, picks the newest one, and
downloads it with libcurl.

## Dependencies vendored in this repo (no network access needed to build)

- `third_party/curl` — libcurl 8.10.1 source, built from source as a static
  library via `add_subdirectory`.
- `third_party/openssl` — OpenSSL 3.5.8 (LTS) source, required for offline
  Linux HTTPS support and built locally by CMake. This is a **pruned** tree
  (~36 MB rather than the full ~140 MB); see
  `third_party/openssl/VENDORING.md` for what was removed and how to
  regenerate or bump it via `scripts/vendor-openssl.sh`.
- `third_party/json` — nlohmann/json v3.11.3 single-header library.

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

## Running

```powershell
./build/Release/jlp_downloader.exe [folderUrl] [outputDir]
```

Both arguments are optional:
- `folderUrl` defaults to the JLPTool Artifactory folder above.
- `outputDir` defaults to the current directory.
