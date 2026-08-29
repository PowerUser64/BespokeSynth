# Download and detect LLVM

# Promote directory contents from SRC_DIR into DEST_DIR, overwriting existing files.
function(bespoke_promote_contents SRC_DIR DEST_DIR)
   file(GLOB _PROMOTE_CONTENTS "${SRC_DIR}/*")
   foreach(_FILE ${_PROMOTE_CONTENTS})
      get_filename_component(_BASENAME "${_FILE}" NAME)
      if(EXISTS "${DEST_DIR}/${_BASENAME}")
         file(REMOVE "${DEST_DIR}/${_BASENAME}")
      endif()
      execute_process(COMMAND ${CMAKE_COMMAND} -E rename "${_FILE}" "${DEST_DIR}/${_BASENAME}"
                      RESULT_VARIABLE _MOVE_RESULT)
      if(NOT _MOVE_RESULT EQUAL 0)
         message(FATAL_ERROR "BESPOKE_DOWNLOAD_LLVM: Failed to promote ${_FILE}")
      endif()
   endforeach()
   file(REMOVE_RECURSE "${SRC_DIR}")
endfunction()

# Find LLVM in the installed directory.
function(bespoke_find_installed_llvm INSTALL_DIR)
   # Clear any cached LLVM_DIR from previous find_package calls
   unset(LLVM_DIR CACHE)
   set(_LLVM_CONFIG_FILE "${INSTALL_DIR}/lib/cmake/llvm/LLVMConfig.cmake")
   if(EXISTS "${_LLVM_CONFIG_FILE}")
      set(LLVM_DIR "${INSTALL_DIR}/lib/cmake/llvm" PARENT_SCOPE)
      set(LLVM_FOUND TRUE PARENT_SCOPE)
      # Read version from the config file
      file(READ "${_LLVM_CONFIG_FILE}" _LLVM_CONFIG_CONTENT)
      string(REGEX MATCH "set\\(LLVM_PACKAGE_VERSION \"([0-9.]+)\"" _LLVM_VERSION_MATCH "${_LLVM_CONFIG_CONTENT}")
      if(_LLVM_VERSION_MATCH)
         set(LLVM_PACKAGE_VERSION "${CMAKE_MATCH_1}" PARENT_SCOPE)
      endif()
   else()
      set(LLVM_FOUND FALSE PARENT_SCOPE)
   endif()
endfunction()

function(bespoke_obtain_llvm MODE)
   if(NOT MODE STREQUAL "detect" AND NOT MODE STREQUAL "always" AND NOT MODE STREQUAL "never")
      message(FATAL_ERROR "BESPOKE_DOWNLOAD_LLVM mode must be 'detect', 'always', or 'never', got '${MODE}'")
   endif()

   # Test for system LLVM 18
   find_package(LLVM 18.1 CONFIG QUIET)
   if(LLVM_FOUND)
      message(STATUS "BESPOKE_DOWNLOAD_LLVM: Found system LLVM ${LLVM_PACKAGE_VERSION} at ${LLVM_DIR}.")
      set(BESPOKE_LLVM_DOWNLOADED OFF CACHE BOOL "" FORCE)
      return()
   endif()

   # Find llvm through llvm-config binary
   find_program(LLVM_CONFIG NAME llvm-config)
   if(LLVM_CONFIG)
      execute_process(
         COMMAND ${LLVM_CONFIG} --cmakedir
         OUTPUT_VARIABLE _LLVM_CMAKE_DIR
         OUTPUT_STRIP_TRAILING_WHITESPACE
         RESULT_VARIABLE _LLVM_CONFIG_RESULT
      )
      if(_LLVM_CONFIG_RESULT EQUAL 0 AND EXISTS "${_LLVM_CMAKE_DIR}/LLVMConfig.cmake")
         find_package(LLVM 18.1 CONFIG HINTS "${_LLVM_CMAKE_DIR}" NO_DEFAULT_PATH QUIET)
         if(LLVM_FOUND)
            message(STATUS "BESPOKE_DOWNLOAD_LLVM: Found system LLVM ${LLVM_PACKAGE_VERSION} at ${LLVM_DIR} (via llvm-config)")
            set(BESPOKE_LLVM_DOWNLOADED OFF CACHE BOOL "" FORCE)
            return()
         endif()
      endif()
   endif()


   if(MODE STREQUAL "never")
      if(NOT LLVM_FOUND)
         message(FATAL_ERROR
         "BESPOKE_DOWNLOAD_LLVM: mode=${MODE} (llvm_system), system LLVM not found.\n"
         "Install LLVM development packages (e.g. libllvm-dev) or use a different BESPOKE_FAUST_BACKEND.")
      endif()
      set(BESPOKE_LLVM_DOWNLOADED OFF CACHE BOOL "" FORCE)
      return()
   endif()

   message(STATUS "BESPOKE_DOWNLOAD_LLVM: mode=${MODE}, system LLVM not found. Downloading LLVM ...")

   # Detect platform and arch
   set(_ARCH "${CMAKE_SYSTEM_PROCESSOR}")
   if(_ARCH MATCHES "x86_64|amd64|AMD64")
      set(_ARCH "x86_64")
   elseif(_ARCH MATCHES "aarch64|arm64|ARM64")
      set(_ARCH "aarch64")
   else()
      message(FATAL_ERROR "BESPOKE_DOWNLOAD_LLVM: Unsupported architecture '${_ARCH}'.")
   endif()

   set(_LLVM_VERSION "18.1.8")

   set(_TARBALLS "")
   set(_HASHES "")
   if(WIN32)
      list(APPEND _TARBALLS "clang+llvm-${_LLVM_VERSION}-${_ARCH}-pc-windows-msvc.tar.xz")
      list(APPEND _HASHES "22c5907db053026cc2a8ff96d21c0f642a90d24d66c23c6d28ee7b1d572b82e8")
   elseif(APPLE)
      if(_ARCH STREQUAL "arm64")
         list(APPEND _TARBALLS "clang+llvm-${_LLVM_VERSION}-${_ARCH}-apple-macos11.tar.xz")
         list(APPEND _HASHES "4573b7f25f46d2a9c8882993f091c52f416c83271db6f5b213c93f0bd0346a10")
      else()
         message(FATAL_ERROR "BESPOKE_DOWNLOAD_LLVM: LLVM 18 does not offer downloads for x86_64 MacOS")
      endif()
   else()
      if(_ARCH STREQUAL "aarch64")
         list(APPEND _TARBALLS "clang+llvm-${_LLVM_VERSION}-${_ARCH}-linux-gnu.tar.xz")
         list(APPEND _HASHES "dcaa1bebbfbb86953fdfbdc7f938800229f75ad26c5c9375ef242edad737d999")
      else()
         list(APPEND _TARBALLS "clang+llvm-${_LLVM_VERSION}-${_ARCH}-linux-gnu-ubuntu-18.04.tar.xz")
         list(APPEND _HASHES "54ec30358afcc9fb8aa74307db3046f5187f9fb89fb37064cdde906e062ebf36")
      endif()
   endif()

   list(GET _TARBALLS 0 _TARBALL)
   list(GET _HASHES 0 _EXPECTED_HASH)
   set(_URL "https://github.com/llvm/llvm-project/releases/download/llvmorg-${_LLVM_VERSION}/${_TARBALL}")

   # Download or use cached tarball
   if(NOT EXISTS "${BESPOKE_LLVM_INSTALL_DIR}/${_TARBALL}")
      message(STATUS "BESPOKE_DOWNLOAD_LLVM: Downloading ${_TARBALL} ...")
      file(DOWNLOAD "${_URL}" "${BESPOKE_LLVM_INSTALL_DIR}/${_TARBALL}" SHOW_PROGRESS STATUS _DL_STATUS)
      list(GET _DL_STATUS 0 _DL_CODE)
      if(NOT _DL_CODE EQUAL 0)
         list(GET _DL_STATUS 1 _DL_MSG)
         message(FATAL_ERROR "BESPOKE_DOWNLOAD_LLVM: Failed to download ${_URL}: ${_DL_MSG}")
      endif()
   endif()

   # Verify tarball integrity
   file(SHA256 "${BESPOKE_LLVM_INSTALL_DIR}/${_TARBALL}" _ACTUAL_HASH)
   if(NOT _ACTUAL_HASH STREQUAL _EXPECTED_HASH)
      message(FATAL_ERROR
            "BESPOKE_DOWNLOAD_LLVM: sha256 mismatch for ${_TARBALL}\n"
            "    Expected:  ${_EXPECTED_HASH}\n"
            "    Actual:    ${_ACTUAL_HASH}")
   endif()

   # Compute expected extraction directory name once
   string(REGEX REPLACE "\\.(tar\\.xz|tar\\.gz|zip)$" "" _EXPECTED_DIR "${_TARBALL}")

   # Check for already-installed LLVM (promoted or unpromoted)
   set(_LLVM_ALREADY_INSTALLED OFF)
   if(EXISTS "${BESPOKE_LLVM_INSTALL_DIR}/lib/cmake/llvm/LLVMConfig.cmake")
      set(_LLVM_ALREADY_INSTALLED ON)
   elseif(EXISTS "${BESPOKE_LLVM_INSTALL_DIR}/bin/llvm-config")
      set(_LLVM_ALREADY_INSTALLED ON)
   elseif(EXISTS "${BESPOKE_LLVM_INSTALL_DIR}/${_EXPECTED_DIR}/lib/cmake/llvm/LLVMConfig.cmake")
      set(_LLVM_ALREADY_INSTALLED ON)
   endif()

   if(_LLVM_ALREADY_INSTALLED)
      message(STATUS "BESPOKE_DOWNLOAD_LLVM: LLVM already installed at ${BESPOKE_LLVM_INSTALL_DIR}, skipping extraction.")

      # Promote unpromoted installation if needed
      if(NOT EXISTS "${BESPOKE_LLVM_INSTALL_DIR}/lib/cmake/llvm/LLVMConfig.cmake")
         bespoke_promote_contents("${BESPOKE_LLVM_INSTALL_DIR}/${_EXPECTED_DIR}" "${BESPOKE_LLVM_INSTALL_DIR}")
      endif()

      bespoke_find_installed_llvm("${BESPOKE_LLVM_INSTALL_DIR}")
      if(LLVM_FOUND)
         message(STATUS "BESPOKE_DOWNLOAD_LLVM: Found LLVM ${LLVM_PACKAGE_VERSION} at ${BESPOKE_LLVM_INSTALL_DIR}")
         set(BESPOKE_LLVM_DOWNLOADED ON CACHE BOOL "" FORCE)
         set(BESPOKE_LLVM_INSTALL_DIR "${BESPOKE_LLVM_INSTALL_DIR}" PARENT_SCOPE)
         # Create wrapper scripts if they don't exist
         if(WIN32)
            if(NOT EXISTS "${BESPOKE_LLVM_INSTALL_DIR}/bin/llvm-config-wrapper.bat")
               file(WRITE "${BESPOKE_LLVM_INSTALL_DIR}/bin/llvm-config-wrapper.bat"
                        "@echo off\r\nset \"PATH=${BESPOKE_LLVM_INSTALL_DIR}/lib;%PATH%\"\r\n${BESPOKE_LLVM_INSTALL_DIR}\\bin\\llvm-config.exe %*\r\n")
            endif()
         else()
            if(NOT EXISTS "${BESPOKE_LLVM_INSTALL_DIR}/bin/llvm-config-wrapper.sh")
               file(WRITE "${BESPOKE_LLVM_INSTALL_DIR}/bin/llvm-config-wrapper.sh"
                    "#!/bin/sh\nexport LD_LIBRARY_PATH=\"${BESPOKE_LLVM_INSTALL_DIR}/lib\"\nexec ${BESPOKE_LLVM_INSTALL_DIR}/bin/llvm-config \"\$@\"\n")
               file(CHMOD "${BESPOKE_LLVM_INSTALL_DIR}/bin/llvm-config-wrapper.sh"
                    PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
            endif()
         endif()
         return()
      endif()
   endif()

   # Clear install directory (except the tarball itself)
   file(GLOB _EXISTING_FILES "${BESPOKE_LLVM_INSTALL_DIR}/*")
   foreach(_FILE ${_EXISTING_FILES})
      get_filename_component(_BASENAME "${_FILE}" NAME)
      if(NOT _BASENAME STREQUAL "${_TARBALL}")
         if(IS_DIRECTORY "${_FILE}")
            file(REMOVE_RECURSE "${_FILE}")
         else()
            file(REMOVE "${_FILE}")
         endif()
      endif()
   endforeach()

   execute_process(
        COMMAND ${CMAKE_COMMAND} -E tar xf "${BESPOKE_LLVM_INSTALL_DIR}/${_TARBALL}"
        WORKING_DIRECTORY "${BESPOKE_LLVM_INSTALL_DIR}"
        RESULT_VARIABLE _EXTRACT_RESULT
   )
   if(NOT _EXTRACT_RESULT EQUAL 0)
      message(FATAL_ERROR "BESPOKE_DOWNLOAD_LLVM: Failed to extract ${_TARBALL}")
   endif()

   # Promote extracted top-level directory contents to INSTALL_DIR
   file(GLOB _ALL_ITEMS "${BESPOKE_LLVM_INSTALL_DIR}/*")
   foreach(_ITEM ${_ALL_ITEMS})
      if(IS_DIRECTORY "${_ITEM}")
         get_filename_component(_ITEM_NAME "${_ITEM}" NAME)
         if(_ITEM_NAME STREQUAL _EXPECTED_DIR)
            bespoke_promote_contents("${_ITEM}" "${BESPOKE_LLVM_INSTALL_DIR}")
            break()
         endif()
      endif()
   endforeach()

   # Verify installation via find_package
   bespoke_find_installed_llvm("${BESPOKE_LLVM_INSTALL_DIR}")
   if(NOT LLVM_FOUND)
      message(FATAL_ERROR
            "BESPOKE_DOWNLOAD_LLVM: LLVM installation at ${BESPOKE_LLVM_INSTALL_DIR} is invalid.\n"
            "Could not find LLVMConfig.cmake. The download may have failed or the archive may be corrupt.")
   endif()

   # Search for version-specific llvm-config (Debian/Ubuntu style)
   string(REGEX REPLACE "\\..*$" "" _BESPOKE_LLVM_VERSION_MAJOR "${BESPOKE_LLVM_VERSION}")
   find_program(LLVM_CONFIG_VERSIONED
      NAMES "llvm-config-${BESPOKE_LLVM_VERSION}"
            "llvm-config-${_BESPOKE_LLVM_VERSION_MAJOR}"
      PATHS "${BESPOKE_LLVM_INSTALL_DIR}/bin"
   )

   if(LLVM_CONFIG_VERSIONED)
      set(LLVM_CONFIG "${LLVM_CONFIG_VERSIONED}")
      message(STATUS "BESPOKE_DOWNLOAD_LLVM: Using version-specific llvm-config: ${LLVM_CONFIG}")
   else()
      # Fall back to generic llvm-config
      find_program(LLVM_CONFIG NAMES llvm-config PATHS "${BESPOKE_LLVM_INSTALL_DIR}/bin" NO_DEFAULT_PATH)
      if(NOT LLVM_CONFIG)
         message(FATAL_ERROR "BESPOKE_DOWNLOAD_LLVM: Could not find llvm-config in ${BESPOKE_LLVM_INSTALL_DIR}/bin")
      endif()
      message(STATUS "BESPOKE_DOWNLOAD_LLVM: Using generic llvm-config: ${LLVM_CONFIG}")
   endif()

   # Verify LLVM version matches expected
   execute_process(
      COMMAND "${LLVM_CONFIG}" --version
      OUTPUT_VARIABLE _DOWNLOADED_LLVM_VERSION
      OUTPUT_STRIP_TRAILING_WHITESPACE
      RESULT_VARIABLE _VERSION_RESULT
   )
   if(NOT _VERSION_RESULT EQUAL 0)
      message(FATAL_ERROR "BESPOKE_DOWNLOAD_LLVM: Failed to get LLVM version from ${LLVM_CONFIG}")
   endif()

   if(NOT _DOWNLOADED_LLVM_VERSION STREQUAL BESPOKE_LLVM_VERSION)
      message(FATAL_ERROR
        "BESPOKE_DOWNLOAD_LLVM: LLVM version mismatch.\n"
        "Expected: ${BESPOKE_LLVM_VERSION}\n"
        "Downloaded: ${_DOWNLOADED_LLVM_VERSION}\n"
        "The downloaded archive may be corrupt or the wrong version.")
   endif()

   message(STATUS "BESPOKE_DOWNLOAD_LLVM: Found LLVM ${_DOWNLOADED_LLVM_VERSION} at ${BESPOKE_LLVM_INSTALL_DIR}")

   set(BESPOKE_LLVM_DOWNLOADED ON CACHE BOOL "" FORCE)
   set(BESPOKE_LLVM_INSTALL_DIR "${BESPOKE_LLVM_INSTALL_DIR}" PARENT_SCOPE)

   message(STATUS "BESPOKE_DOWNLOAD_LLVM: LLVM installed at ${BESPOKE_LLVM_INSTALL_DIR}")
endfunction()