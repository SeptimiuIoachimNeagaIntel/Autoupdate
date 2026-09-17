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
- `third_party/json` — nlohmann/json v3.11.3 single-header library.

Both are committed to the repository, so building this project does not
require internet access or a package manager.

## Building

CMake auto-detects the compiler (Visual Studio on Windows, GCC/Clang on
Linux) — no toolchain is specified explicitly.

```powershell
cmake -S . -B build
cmake --build build --config Release
```

On Windows, TLS is provided by the native Schannel backend (no extra
dependency). On Linux/macOS, libcurl is configured to use OpenSSL, which
must be available on the system (e.g. `libssl-dev` on Debian/Ubuntu,
`openssl-devel` on Fedora/RHEL) — this is a local `find_package()` lookup,
not a download.

## Running

```powershell
./build/Release/jlp_downloader.exe [folderUrl] [outputDir]
```

Both arguments are optional:
- `folderUrl` defaults to the JLPTool Artifactory folder above.
- `outputDir` defaults to the current directory.
