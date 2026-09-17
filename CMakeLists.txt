cmake_minimum_required(VERSION 3.16)
project(JLPToolDownloader LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---------------------------------------------------------------------------
# Vendored libcurl (third_party/curl), built from source, no network access
# required at configure/build time. Options below are set before adding the
# subdirectory so they take effect for curl's own CMakeLists.txt.
# ---------------------------------------------------------------------------
set(BUILD_CURL_EXE OFF CACHE BOOL "Build curl executable" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build libcurl as a static library" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "Build curl tests" FORCE)
set(CURL_DISABLE_INSTALL ON CACHE BOOL "Disable curl install targets" FORCE)

if(WIN32)
  # Use the OS-native TLS backend on Windows so no extra TLS library needs
  # to be vendored or installed.
  set(CURL_USE_SCHANNEL ON CACHE BOOL "Use native Windows TLS backend" FORCE)
else()
  # On Linux/macOS, rely on the system-installed OpenSSL development
  # package (e.g. libssl-dev / openssl-devel). This is a local lookup via
  # find_package(), it does not download anything.
  set(CURL_USE_OPENSSL ON CACHE BOOL "Use OpenSSL TLS backend" FORCE)
endif()

add_subdirectory(third_party/curl EXCLUDE_FROM_ALL)

add_executable(jlp_downloader src/main.cpp)

target_link_libraries(jlp_downloader PRIVATE libcurl)
target_include_directories(jlp_downloader PRIVATE third_party/json)
