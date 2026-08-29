add_subdirectory(abletonmove EXCLUDE_FROM_ALL)
add_subdirectory(exprtk EXCLUDE_FROM_ALL)
add_subdirectory(freeverb EXCLUDE_FROM_ALL)
if(NOT BESPOKE_SYSTEM_JSONCPP)
  add_subdirectory(json EXCLUDE_FROM_ALL)
  message(STATUS "Using bundled jsoncpp")
endif()
add_subdirectory(leathers EXCLUDE_FROM_ALL)
add_subdirectory(nanovg EXCLUDE_FROM_ALL)
add_subdirectory(oddsound-mts EXCLUDE_FROM_ALL)
add_subdirectory(psmove EXCLUDE_FROM_ALL)
add_subdirectory(push2 EXCLUDE_FROM_ALL)
if(NOT BESPOKE_SYSTEM_READERWRITERQUEUE)
  add_subdirectory(readerwriterqueue EXCLUDE_FROM_ALL)
  message(STATUS "Using bundled readerwriterqueue")
endif()
add_subdirectory(scalable-font2 EXCLUDE_FROM_ALL)

set(PYBIND11_NOPYTHON TRUE)
if(NOT BESPOKE_SYSTEM_PYBIND11)
  message(STATUS "Using bundled pybind11")
  add_subdirectory(pybind11 EXCLUDE_FROM_ALL)
else()
  message(STATUS "Using system installed pybind11")
  include(FindPkgConfig)
  find_package(pybind11 COMPONENTS REQUIRED)
endif()
add_library(bespoke_pybind11_wrapper INTERFACE)
target_compile_definitions(bespoke_pybind11_wrapper INTERFACE PYBIND11_EXPORT=)
target_link_libraries(bespoke_pybind11_wrapper INTERFACE pybind11::pybind11)
add_library(bespoke::pybind11 ALIAS bespoke_pybind11_wrapper)

if(NOT BESPOKE_SYSTEM_TUNING_LIBRARY)
  add_subdirectory(tuning-library EXCLUDE_FROM_ALL)
  message(STATUS "Using bundled tuning-library")
else()
  find_file(TUNING_LIB_TUNINGS NAMES Tunings.h PATHS /usr/local/include /usr/include REQUIRED)
  find_file(TUNING_LIB_TUNINGS_IMPL NAMES TuningsImpl.h PATHS /usr/local/include /usr/include REQUIRED)
  message(STATUS "Using system provided tuning-library")
endif()
add_subdirectory(xwax EXCLUDE_FROM_ALL)

if (NOT BESPOKE_USE_SYSTEM_FAUST)
  message(STATUS "Using bundled libfaust")
    set(INCLUDE_STATIC     ON CACHE BOOL "" FORCE)
    set(INCLUDE_EXECUTABLE OFF)
    set(INCLUDE_OSC        OFF)
    set(INCLUDE_HTTP       OFF)
    set(INCLUDE_EMCC       OFF)
    set(INCLUDE_WASM_GLUE  OFF)
  if(BESPOKE_FAUST_BACKEND STREQUAL "interpreter")
    message(STATUS "Faust interpreter backend enabled")
    include(${CMAKE_CURRENT_SOURCE_DIR}/faust/build/backends/interp.cmake)
    set(INCLUDE_LLVM      OFF CACHE BOOL   "" FORCE)
    set(LLVM_INCLUDE_DIRS ""  CACHE STRING "" FORCE)
  else()
    message(STATUS "Faust LLVM backend enabled")
    set(INCLUDE_LLVM       ON CACHE BOOL "" FORCE)
    set(LLVM_BACKEND COMPILER STATIC DYNAMIC)
  endif()

  # If we downloaded LLVM, point faust's build at it and prevent its
  # llvm_config() macro from re-running (which would FATAL_ERROR without
  # system LLVM).  All LLVM_* variables were already set by DownloadLLVM.cmake.
  if(BESPOKE_FAUST_BACKEND STREQUAL "interpreter")
    # Interpreter backend doesn't need LLVM variables
  else()
    if(BESPOKE_LLVM_DOWNLOADED)
      set(LLVM_DIR "${BESPOKE_LLVM_INSTALL_DIR}/lib/cmake/llvm" CACHE PATH "" FORCE)
      set(LLVM_INCLUDE_DIRS "${BESPOKE_LLVM_INSTALL_DIR}/include" CACHE PATH "" FORCE)
      set(LLVM_LIB_DIR "${BESPOKE_LLVM_INSTALL_DIR}/lib" CACHE PATH "" FORCE)
      set(USE_LLVM_CONFIG OFF CACHE BOOL "" FORCE)
    else()
      # System LLVM: use system llvm-config directly
      find_program(LLVM_CONFIG NAMES llvm-config)
      if(LLVM_CONFIG)
        execute_process(
          COMMAND ${LLVM_CONFIG} --cmakedir
          OUTPUT_VARIABLE _SYSTEM_LLVM_CMAKEDIR
          OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        execute_process(
          COMMAND ${LLVM_CONFIG} --includedir
          OUTPUT_VARIABLE _SYSTEM_LLVM_INCLUDEDIR
          OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        execute_process(
          COMMAND ${LLVM_CONFIG} --libdir
          OUTPUT_VARIABLE _SYSTEM_LLVM_LIBDIR
          OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        set(LLVM_CONFIG "${LLVM_CONFIG}" CACHE FILEPATH "" FORCE)
        set(LLVM_DIR "${_SYSTEM_LLVM_CMAKEDIR}" CACHE PATH "" FORCE)
        set(LLVM_INCLUDE_DIRS "${_SYSTEM_LLVM_INCLUDEDIR}" CACHE PATH "" FORCE)
        set(LLVM_LIB_DIR "${_SYSTEM_LLVM_LIBDIR}" CACHE PATH "" FORCE)
      endif()
    endif()
  endif()

  add_subdirectory(
    ${CMAKE_CURRENT_SOURCE_DIR}/faust/build
    ${CMAKE_BINARY_DIR}/faust
    )

  # Embed LLVM static libraries into libfaust when using downloaded LLVM
  if(BESPOKE_FAUST_BACKEND STREQUAL "interpreter")
    # Interpreter backend doesn't need LLVM embedding
  else()
    if(BESPOKE_LLVM_DOWNLOADED)
      if(WIN32)
        set(_llvm_wrapper "llvm-config-wrapper.bat")
      else()
        set(_llvm_wrapper "llvm-config-wrapper.sh")
      endif()
      add_custom_command(
        OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/faust/build/lib/libfaustwithllvm.a
        COMMAND ${CMAKE_COMMAND} -E env
          "LLVM_CONFIG=${BESPOKE_LLVM_INSTALL_DIR}/bin/${_llvm_wrapper}"
          "LLVM_LIB_DIR=${BESPOKE_LLVM_INSTALL_DIR}/lib"
          make -f Make.llvm.static
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/faust/build
        COMMENT "Embed LLVM static libraries with libfaust (using downloaded LLVM)"
        VERBATIM
      )
      add_custom_target(embed_llvm_in_faust ALL DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/faust/build/lib/libfaustwithllvm.a)

      if(TARGET staticlib)
        add_dependencies(embed_llvm_in_faust staticlib)
      endif()
    endif()
  endif()

else()
  message(STATUS "Using system-provided libfaust")
endif()