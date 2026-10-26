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
        set(LLVM_LIBS "${_llvm_archives}" PARENT_SCOPE)
    else()
        # Headers need the __STDC_* macros so LLVM's generated config headers parse.
        set(LLVM_DEFINITIONS "-D__STDC_CONSTANT_MACROS -D__STDC_FORMAT_MACROS -D__STDC_LIMIT_MACROS" PARENT_SCOPE)
        set(LLVM_LD_FLAGS "-L${_llvm_dir}/lib" PARENT_SCOPE)
        # The static LLVM archives are what the GitHub prebuilt tarballs ship (there
        # is no shared libLLVM.so), so collect them all as absolute paths.
        file(GLOB _llvm_archives "${_llvm_dir}/lib/libLLVM*.a")
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
