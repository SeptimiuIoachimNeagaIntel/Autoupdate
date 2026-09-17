include(CMakeParseArguments)
include(ProcessorCount)

function(jlp_build_bundled_openssl)
  set(oneValueArgs SOURCE_DIR BUILD_DIR INSTALL_DIR)
  cmake_parse_arguments(JLP_OPENSSL "" "${oneValueArgs}" "" ${ARGN})

  if(NOT JLP_OPENSSL_SOURCE_DIR OR NOT JLP_OPENSSL_BUILD_DIR OR NOT JLP_OPENSSL_INSTALL_DIR)
    message(FATAL_ERROR "jlp_build_bundled_openssl requires SOURCE_DIR, BUILD_DIR, and INSTALL_DIR")
  endif()

  if(NOT EXISTS "${JLP_OPENSSL_SOURCE_DIR}/Configure")
    message(FATAL_ERROR
      "Offline Linux builds require OpenSSL source at third_party/openssl, "
      "but no Configure file was found there. Re-vendor it by running "
      "scripts/vendor-openssl.sh (needs network access once); CMake then builds "
      "it locally and never uses a system OpenSSL package.")
  endif()

  find_program(PERL_EXECUTABLE NAMES perl REQUIRED)
  find_program(JLP_OPENSSL_MAKE_EXECUTABLE NAMES make gmake REQUIRED)

  string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" JLP_OPENSSL_PROCESSOR)
  if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    if(JLP_OPENSSL_PROCESSOR MATCHES "^(x86_64|amd64)$")
      set(JLP_OPENSSL_TARGET linux-x86_64)
    elseif(JLP_OPENSSL_PROCESSOR MATCHES "^(aarch64|arm64)$")
      set(JLP_OPENSSL_TARGET linux-aarch64)
    elseif(JLP_OPENSSL_PROCESSOR MATCHES "^(armv7|armv7l)$")
      set(JLP_OPENSSL_TARGET linux-armv4)
    elseif(JLP_OPENSSL_PROCESSOR MATCHES "^(i[3-6]86|x86)$")
      set(JLP_OPENSSL_TARGET linux-x86)
    else()
      message(FATAL_ERROR "Unsupported Linux processor for bundled OpenSSL: ${CMAKE_SYSTEM_PROCESSOR}")
    endif()
  else()
    message(FATAL_ERROR "Bundled OpenSSL is currently configured only for Linux builds")
  endif()

  set(JLP_OPENSSL_STAMP "${JLP_OPENSSL_INSTALL_DIR}/.jlp-openssl-${JLP_OPENSSL_TARGET}.stamp")
  if(NOT EXISTS "${JLP_OPENSSL_STAMP}")
    ProcessorCount(JLP_OPENSSL_JOB_COUNT)
    if(NOT JLP_OPENSSL_JOB_COUNT)
      set(JLP_OPENSSL_JOB_COUNT 1)
    endif()

    file(MAKE_DIRECTORY "${JLP_OPENSSL_BUILD_DIR}")
    file(MAKE_DIRECTORY "${JLP_OPENSSL_INSTALL_DIR}")

    message(STATUS "Building bundled OpenSSL for ${JLP_OPENSSL_TARGET}")
    # The vendored tree is pruned (see third_party/openssl/VENDORING.md): the
    # test, doc, demos, fuzz and apps directories are stripped. doc, fuzz and
    # apps additionally keep an empty build.info stub, because the top-level
    # build.info lists those three in SUBDIRS unconditionally and Configure
    # reads each one regardless of the no-* flags. The flags below must stay in
    # sync with that prune, so Configure emits no rules for absent sources.
    execute_process(
      COMMAND "${PERL_EXECUTABLE}" "${JLP_OPENSSL_SOURCE_DIR}/Configure" ${JLP_OPENSSL_TARGET}
              no-shared no-tests no-docs no-apps no-demos
              "--prefix=${JLP_OPENSSL_INSTALL_DIR}" "--openssldir=${JLP_OPENSSL_INSTALL_DIR}/ssl"
      WORKING_DIRECTORY "${JLP_OPENSSL_BUILD_DIR}"
      RESULT_VARIABLE JLP_OPENSSL_CONFIGURE_RESULT
    )
    if(NOT JLP_OPENSSL_CONFIGURE_RESULT EQUAL 0)
      message(FATAL_ERROR "Bundled OpenSSL configure failed")
    endif()

    execute_process(
      COMMAND "${JLP_OPENSSL_MAKE_EXECUTABLE}" -j${JLP_OPENSSL_JOB_COUNT} build_sw
      WORKING_DIRECTORY "${JLP_OPENSSL_BUILD_DIR}"
      RESULT_VARIABLE JLP_OPENSSL_BUILD_RESULT
    )
    if(NOT JLP_OPENSSL_BUILD_RESULT EQUAL 0)
      message(FATAL_ERROR "Bundled OpenSSL build failed")
    endif()

    execute_process(
      COMMAND "${JLP_OPENSSL_MAKE_EXECUTABLE}" install_sw
      WORKING_DIRECTORY "${JLP_OPENSSL_BUILD_DIR}"
      RESULT_VARIABLE JLP_OPENSSL_INSTALL_RESULT
    )
    if(NOT JLP_OPENSSL_INSTALL_RESULT EQUAL 0)
      message(FATAL_ERROR "Bundled OpenSSL install failed")
    endif()

    file(WRITE "${JLP_OPENSSL_STAMP}" "${JLP_OPENSSL_TARGET}\n")
  endif()

  set(JLP_BUNDLED_OPENSSL_ROOT "${JLP_OPENSSL_INSTALL_DIR}" PARENT_SCOPE)
endfunction()