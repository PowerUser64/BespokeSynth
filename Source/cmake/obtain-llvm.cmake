# Downloads and extracts a prebuilt LLVM from the llvm-project GitHub
# releases into ${BESPOKE_LLVM_INSTALL_DIR}, then sets the LLVM_* variables
# consumed by the bundled Faust build and BespokeSynth.
#
# The bundled Faust (libs/faust) only supports LLVM up to version 22, and the
# GitHub releases ship prebuilt LLVM tarballs (LLVM-<version>-Linux-X64, etc.)
# for the major platforms.  See AGENTS.md: libs/faust is not our code, so the
# version must stay within what that bundled Faust supports.

set(BESPOKE_LLVM_VERSION "22.1.8" CACHE STRING
    "LLVM version to download when BESPOKE_FAUST_BACKEND is 'llvm_download'")
set(BESPOKE_LLVM_INSTALL_DIR "${CMAKE_SOURCE_DIR}/ignore/llvm" CACHE PATH
    "Directory where the downloaded LLVM is extracted")

function(bespoke_download_llvm)
    # Pick the platform-specific prebuilt package.  The LLVM-only prebuilt
    # tarballs are named LLVM-<version>-<platform>.tar.xz on the GitHub
    # releases page; on Windows the package is clang+llvm-<version>-
    # <arch>-pc-windows-msvc.tar.xz (there is no LLVM-only tarball for
    # Windows, only .exe/.msi installers).
    if(APPLE)
        if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm|aarch64|arm64)")
            set(_llvm_pkg "LLVM-${BESPOKE_LLVM_VERSION}-macOS-ARM64.tar.xz")
        else()
            set(_llvm_pkg "LLVM-${BESPOKE_LLVM_VERSION}-macOS-X64.tar.xz")
        endif()
    elseif(WIN32)
        if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm|aarch64|arm64)")
            set(_llvm_pkg "clang+llvm-${BESPOKE_LLVM_VERSION}-aarch64-pc-windows-msvc.tar.xz")
        else()
            set(_llvm_pkg "clang+llvm-${BESPOKE_LLVM_VERSION}-x86_64-pc-windows-msvc.tar.xz")
        endif()
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm|aarch64|arm64)")
        set(_llvm_pkg "LLVM-${BESPOKE_LLVM_VERSION}-Linux-ARM64.tar.xz")
    else()
        set(_llvm_pkg "LLVM-${BESPOKE_LLVM_VERSION}-Linux-X64.tar.xz")
    endif()

    set(_llvm_url
        "https://github.com/llvm/llvm-project/releases/download/llvmorg-${BESPOKE_LLVM_VERSION}/${_llvm_pkg}")
    set(_llvm_tarball "${BESPOKE_LLVM_INSTALL_DIR}/${_llvm_pkg}")
    # The tarball extracts to a directory named after the package (minus .tar.xz).
    string(REGEX REPLACE "\\.tar\\.xz$" "" _llvm_extract "${_llvm_pkg}")
    set(_llvm_dir "${BESPOKE_LLVM_INSTALL_DIR}/${_llvm_extract}")

    # Download and extract only if the extracted LLVM isn't already present.
    if(NOT EXISTS "${_llvm_dir}/include/llvm" OR NOT EXISTS "${_llvm_dir}/lib")
        file(MAKE_DIRECTORY "${BESPOKE_LLVM_INSTALL_DIR}")
        if(NOT EXISTS "${_llvm_tarball}")
            message(STATUS "Downloading LLVM ${BESPOKE_LLVM_VERSION} from ${_llvm_url}")
            file(DOWNLOAD "${_llvm_url}" "${_llvm_tarball}"
                 TLS_VERIFY ON
                 STATUS _dl_status)
            list(GET _dl_status 0 _dl_code)
            list(GET _dl_status 1 _dl_error)
            if(NOT _dl_code EQUAL 0)
                message(FATAL_ERROR "Failed to download LLVM ${BESPOKE_LLVM_VERSION} from ${_llvm_url}: ${_dl_error}")
            endif()
        endif()

        message(STATUS "Extracting LLVM ${BESPOKE_LLVM_VERSION} to ${_llvm_dir}")
        # Use the system tar when available; cmake -E tar handles .tar.xz too.
        find_program(_tar_cmd tar)
        if(_tar_cmd)
            execute_process(COMMAND "${_tar_cmd}" -xf "${_llvm_tarball}" -C "${BESPOKE_LLVM_INSTALL_DIR}"
                            RESULT_VARIABLE _extract_result)
        else()
            execute_process(COMMAND "${CMAKE_COMMAND}" -E tar xJf "${_llvm_tarball}"
                            WORKING_DIRECTORY "${BESPOKE_LLVM_INSTALL_DIR}"
                            RESULT_VARIABLE _extract_result)
        endif()
        if(NOT _extract_result EQUAL 0)
            message(FATAL_ERROR "Failed to extract LLVM ${BESPOKE_LLVM_VERSION} from ${_llvm_tarball}")
        endif()
    else()
        message(STATUS "LLVM ${BESPOKE_LLVM_VERSION} already downloaded and extracted at ${_llvm_dir}")
    endif()

    # Expose the LLVM layout to the Faust build.  Faust skips its own LLVM
    # discovery (llvm-config/find_package) when these are all defined.
    set(LLVM_DIR "${_llvm_dir}" PARENT_SCOPE)
    set(LLVM_INCLUDE_DIRS "${_llvm_dir}/include" PARENT_SCOPE)
    set(LLVM_LIB_DIR "${_llvm_dir}/lib" PARENT_SCOPE)
    set(LLVM_PACKAGE_VERSION "${BESPOKE_LLVM_VERSION}" PARENT_SCOPE)

    if(MSVC)
        # MSVC: Faust's discovery condition skips LLVM_DEFINITIONS, and the
        # linker uses /LIBPATH rather than -L.
        set(LLVM_DEFINITIONS "" PARENT_SCOPE)
        set(LLVM_LD_FLAGS "" PARENT_SCOPE)
        # The Windows package ships static LLVM<Component>.lib archives (no
        # shared libLLVM; LLVM-C.dll is C-API only and cannot satisfy Faust's
        # C++ llvm:: references).  Link them all as absolute paths.
        file(GLOB _llvm_archives "${_llvm_dir}/lib/LLVM*.lib")
        list(FILTER _llvm_archives EXCLUDE REGEX ".*LLVM-C\\.lib$")
        bespoke_filter_tool_archives(_llvm_archives)
        set(LLVM_LIBS "${_llvm_archives}" PARENT_SCOPE)
    else()
        # Headers need the __STDC_* macros so LLVM's generated config headers parse.
        set(LLVM_DEFINITIONS "-D__STDC_CONSTANT_MACROS -D__STDC_FORMAT_MACROS -D__STDC_LIMIT_MACROS" PARENT_SCOPE)
        set(LLVM_LD_FLAGS "-L${_llvm_dir}/lib" PARENT_SCOPE)
        # The static LLVM archives are what the GitHub prebuilt tarballs ship (there
        # is no shared libLLVM.so), so collect them all as absolute paths.
        file(GLOB _llvm_archives "${_llvm_dir}/lib/libLLVM*.a")
        bespoke_filter_tool_archives(_llvm_archives)
        if(APPLE)
            # Apple's ld does not support --start-group; pass the archives
            # plainly (may need -Wl,-all_load, untested).
            set(LLVM_LIBS "${_llvm_archives}" PARENT_SCOPE)
        else()
            # GNU ld: wrap the archives in --start-group to resolve the circular
            # dependencies between LLVM components.
            set(LLVM_LIBS "-Wl,--start-group;${_llvm_archives};-Wl,--end-group" PARENT_SCOPE)
        endif()
    endif()
endfunction()

# Drop LLVM component archives that are standalone developer tools and not
# used by Faust's LLVM backend.  This avoids pulling in extra system library
# dependencies (e.g. libxml2, only referenced by libLLVMWindowsManifest) and
# keeps the link line and binary smaller.
function(bespoke_filter_tool_archives ARCHIVE_LIST)
    list(FILTER ${ARCHIVE_LIST} EXCLUDE REGEX "libLLVM(WindowsManifest|WindowsDriver|BOLT[A-Za-z0-9]*|TableGen[A-Za-z0-9]*|FileCheck|Fuzzer[A-Za-z0-9]*|XRay|InterfaceStub)\\.(a|lib)$")
    set(${ARCHIVE_LIST} ${${ARCHIVE_LIST}} PARENT_SCOPE)
endfunction()

# Detect a usable system LLVM.  If one is found, expose its layout the same way
# bespoke_download_llvm() does (LLVM_DIR / LLVM_INCLUDE_DIRS / LLVM_LIB_DIR /
# LLVM_PACKAGE_VERSION / LLVM_DEFINITIONS / LLVM_LD_FLAGS / LLVM_LIBS), so the
# bundled Faust build and BespokeSynth consume it identically.
#
# Detection mirrors the original llvm_system / llvm behavior: first a CONFIG
# find_package for the same LLVM major.minor the downloader ships (LLVM's
# version file requires an exact major.minor match), then a llvm-config
# fallback.  Deriving the static link line requires llvm-config, so a system
# install without llvm-config is treated as not found.
function(bespoke_find_system_llvm)
    # The bundled Faust supports LLVM up to version 22; accept a system LLVM of
    # the same major.minor the downloader provides.
    string(REGEX REPLACE "([0-9]+)\\.([0-9]+).*" "\\1.\\2" _llvm_version "${BESPOKE_LLVM_VERSION}")

    # Clear any cached LLVM_DIR from previous find_package calls.
    unset(LLVM_DIR CACHE)

    set(_system_found OFF)

    # 1. Standard CONFIG search.
    find_package(LLVM ${_llvm_version} CONFIG QUIET)
    if(LLVM_FOUND)
        set(_system_found ON)
    else()
        # 2. llvm-config fallback: point find_package at its cmake dir.
        find_program(_llvm_config NAME llvm-config)
        if(_llvm_config)
            execute_process(COMMAND "${_llvm_config}" --cmakedir
                            OUTPUT_VARIABLE _llvm_cmake_dir
                            OUTPUT_STRIP_TRAILING_WHITESPACE
                            RESULT_VARIABLE _cmake_result)
            if(_cmake_result EQUAL 0 AND EXISTS "${_llvm_cmake_dir}/LLVMConfig.cmake")
                find_package(LLVM ${_llvm_version} CONFIG HINTS "${_llvm_cmake_dir}" NO_DEFAULT_PATH QUIET)
                if(LLVM_FOUND)
                    set(_system_found ON)
                endif()
            endif()
        endif()
    endif()

    if(_system_found)
        # Derive the layout and static link line from llvm-config.  Prefer the
        # install's own llvm-config (LLVMConfig sets LLVM_TOOLS_BINARY_DIR),
        # falling back to one on PATH.
        find_program(_llvm_config NAMES llvm-config HINTS "${LLVM_TOOLS_BINARY_DIR}" NO_DEFAULT_PATH)
        if(NOT _llvm_config)
            find_program(_llvm_config NAMES llvm-config)
        endif()
        if(_llvm_config)
            execute_process(COMMAND "${_llvm_config}" --includedir
                            OUTPUT_VARIABLE _llvm_include
                            OUTPUT_STRIP_TRAILING_WHITESPACE
                            RESULT_VARIABLE _inc_result)
            execute_process(COMMAND "${_llvm_config}" --libdir
                            OUTPUT_VARIABLE _llvm_lib
                            OUTPUT_STRIP_TRAILING_WHITESPACE
                            RESULT_VARIABLE _lib_result)
            execute_process(COMMAND "${_llvm_config}" --version
                            OUTPUT_VARIABLE _llvm_version_out
                            OUTPUT_STRIP_TRAILING_WHITESPACE
                            RESULT_VARIABLE _ver_result)
            execute_process(COMMAND "${_llvm_config}" --ldflags
                            OUTPUT_VARIABLE _llvm_ldflags
                            OUTPUT_STRIP_TRAILING_WHITESPACE
                            RESULT_VARIABLE _ld_result)
            execute_process(COMMAND "${_llvm_config}" --link-static --libs all
                            OUTPUT_VARIABLE _llvm_libs
                            OUTPUT_STRIP_TRAILING_WHITESPACE
                            RESULT_VARIABLE _libs_result)
            execute_process(COMMAND "${_llvm_config}" --system-libs
                            OUTPUT_VARIABLE _llvm_syslibs
                            OUTPUT_STRIP_TRAILING_WHITESPACE
                            RESULT_VARIABLE _sys_result)
            if(_inc_result EQUAL 0 AND _lib_result EQUAL 0 AND _ver_result EQUAL 0
               AND _libs_result EQUAL 0 AND _sys_result EQUAL 0
               AND NOT _llvm_include STREQUAL "" AND NOT _llvm_lib STREQUAL ""
               AND NOT _llvm_version_out STREQUAL "")
                string(REPLACE " " ";" _llvm_libs "${_llvm_libs} ${_llvm_syslibs}")
                if(DEFINED LLVM_DEFINITIONS AND NOT LLVM_DEFINITIONS STREQUAL "")
                    set(_llvm_defs "${LLVM_DEFINITIONS}")
                else()
                    set(_llvm_defs "-D__STDC_CONSTANT_MACROS -D__STDC_FORMAT_MACROS -D__STDC_LIMIT_MACROS")
                endif()
                if(_llvm_ldflags STREQUAL "")
                    set(_llvm_ldflags "-L${_llvm_lib}")
                endif()
                set(BESPOKE_SYSTEM_LLVM_FOUND ON PARENT_SCOPE)
                set(LLVM_DIR "${LLVM_DIR}" PARENT_SCOPE)
                set(LLVM_INCLUDE_DIRS "${_llvm_include}" PARENT_SCOPE)
                set(LLVM_LIB_DIR "${_llvm_lib}" PARENT_SCOPE)
                set(LLVM_PACKAGE_VERSION "${_llvm_version_out}" PARENT_SCOPE)
                set(LLVM_DEFINITIONS "${_llvm_defs}" PARENT_SCOPE)
                set(LLVM_LD_FLAGS "${_llvm_ldflags}" PARENT_SCOPE)
                set(LLVM_LIBS "${_llvm_libs}" PARENT_SCOPE)
            endif()
        endif()
    endif()
endfunction()

# Acquire LLVM for the bundled Faust build.  MODE is one of:
#   'never'   use a system LLVM only (BESPOKE_FAUST_BACKEND=llvm_system)
#   'detect'  use a system LLVM, else download (BESPOKE_FAUST_BACKEND=llvm)
#   'always'  download only (BESPOKE_FAUST_BACKEND=llvm_download)
# Sets BESPOKE_LLVM_DOWNLOADED (ON/OFF) and the LLVM_* layout variables.
function(bespoke_obtain_llvm MODE)
    if(NOT MODE STREQUAL "detect" AND NOT MODE STREQUAL "always" AND NOT MODE STREQUAL "never")
        message(FATAL_ERROR "BESPOKE_FAUST_BACKEND mode must be 'detect', 'always', or 'never', got '${MODE}'")
    endif()

    if(NOT MODE STREQUAL "always")
        bespoke_find_system_llvm()
        if(BESPOKE_SYSTEM_LLVM_FOUND)
            message(STATUS "BESPOKE_OBTAIN_LLVM: Found system LLVM ${LLVM_PACKAGE_VERSION} at ${LLVM_DIR}.")
            set(BESPOKE_LLVM_DOWNLOADED OFF CACHE BOOL "" FORCE)
            # Cache the layout so sibling directory scopes (e.g. Source) can see it.
            set(LLVM_DIR "${LLVM_DIR}" CACHE PATH "" FORCE)
            set(LLVM_INCLUDE_DIRS "${LLVM_INCLUDE_DIRS}" CACHE PATH "" FORCE)
            set(LLVM_LIB_DIR "${LLVM_LIB_DIR}" CACHE PATH "" FORCE)
            # The remaining variables flow libs -> faust -> staticlib ->
            # BespokeSynth, so leave them as plain (non-cache) variables.
            set(LLVM_PACKAGE_VERSION "${LLVM_PACKAGE_VERSION}" PARENT_SCOPE)
            set(LLVM_DEFINITIONS "${LLVM_DEFINITIONS}" PARENT_SCOPE)
            set(LLVM_LD_FLAGS "${LLVM_LD_FLAGS}" PARENT_SCOPE)
            set(LLVM_LIBS "${LLVM_LIBS}" PARENT_SCOPE)
            return()
        endif()
    endif()

    if(MODE STREQUAL "never")
        message(FATAL_ERROR
            "BESPOKE_OBTAIN_LLVM: mode='never' (llvm_system), system LLVM not found.\n"
            "Install LLVM development packages (e.g. libllvm-dev) or use a different BESPOKE_FAUST_BACKEND.")
    endif()

    # Download (llvm_download, or llvm when no system LLVM was found).
    bespoke_download_llvm()
    set(BESPOKE_LLVM_DOWNLOADED ON CACHE BOOL "" FORCE)
    set(LLVM_DIR "${LLVM_DIR}" CACHE PATH "" FORCE)
    set(LLVM_INCLUDE_DIRS "${LLVM_INCLUDE_DIRS}" CACHE PATH "" FORCE)
    set(LLVM_LIB_DIR "${LLVM_LIB_DIR}" CACHE PATH "" FORCE)
    set(LLVM_PACKAGE_VERSION "${LLVM_PACKAGE_VERSION}" PARENT_SCOPE)
    set(LLVM_DEFINITIONS "${LLVM_DEFINITIONS}" PARENT_SCOPE)
    set(LLVM_LD_FLAGS "${LLVM_LD_FLAGS}" PARENT_SCOPE)
    set(LLVM_LIBS "${LLVM_LIBS}" PARENT_SCOPE)
endfunction()
