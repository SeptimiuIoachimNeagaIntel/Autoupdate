cmake_minimum_required(VERSION 3.16)
project(JLPToolDownloader LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# ---------------------------------------------------------------------------
# Vendored libcurl (third_party/curl), built from source, no network access
# required at configure/build time. Options below are set before adding the
# subdirectory so they take effect for curl's own CMakeLists.txt.
# ---------------------------------------------------------------------------
set(BUILD_CURL_EXE OFF CACHE BOOL "Build curl executable" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build libcurl as a static library" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "Build curl tests" FORCE)
set(CURL_DISABLE_TESTS ON CACHE BOOL "Disable curl tests" FORCE)
set(CURL_DISABLE_INSTALL ON CACHE BOOL "Disable curl install targets" FORCE)

# The vendored curl tree is pruned down to the library and its CMake build
# (see third_party/curl/VENDORING.md): docs/, tests/, src/ and the autotools
# and nmake build systems are gone. These options must be off or curl's
# CMakeLists.txt descends into directories that no longer exist. Turning the
# manuals off also drops curl's build-time Perl requirement.
set(BUILD_LIBCURL_DOCS OFF CACHE BOOL "Build libcurl man pages" FORCE)
set(BUILD_MISC_DOCS OFF CACHE BOOL "Build misc curl man pages" FORCE)
set(ENABLE_CURL_MANUAL OFF CACHE BOOL "Build the curl man page" FORCE)
set(BUILD_EXAMPLES OFF CACHE BOOL "Build libcurl examples" FORCE)

if(WIN32)
  # Use the OS-native TLS backend on Windows so no extra TLS library needs
  # to be vendored or installed.
  set(CURL_USE_SCHANNEL ON CACHE BOOL "Use native Windows TLS backend" FORCE)
elseif(APPLE)
  # Use the OS-native TLS backend on macOS so no extra TLS library needs
  # to be vendored or installed.
  set(CURL_USE_SECTRANSP ON CACHE BOOL "Use native macOS TLS backend" FORCE)
else()
  # Linux builds must be offline and not depend on a system OpenSSL package.
  # Build the committed OpenSSL source tree locally, then let curl find it.
  include(BuildBundledOpenSSL)
  jlp_build_bundled_openssl(
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/openssl"
    BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}/third_party/openssl-build"
    INSTALL_DIR "${CMAKE_CURRENT_BINARY_DIR}/third_party/openssl"
  )

  set(OPENSSL_ROOT_DIR "${JLP_BUNDLED_OPENSSL_ROOT}" CACHE PATH "Bundled OpenSSL install root" FORCE)
  set(OPENSSL_USE_STATIC_LIBS ON CACHE BOOL "Use bundled static OpenSSL libraries" FORCE)
  set(CURL_USE_OPENSSL ON CACHE BOOL "Use OpenSSL TLS backend" FORCE)
endif()

add_subdirectory(third_party/curl EXCLUDE_FROM_ALL)

add_executable(jlp_downloader src/main.cpp)

target_link_libraries(jlp_downloader PRIVATE libcurl)
target_include_directories(jlp_downloader PRIVATE third_party/json)
