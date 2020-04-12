cmake_minimum_required(VERSION 2.8)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  if(DEFINED ENV{VITASDK})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake" CACHE PATH "toolchain file")
  else()
    message(FATAL_ERROR "Please define VITASDK to point to your SDK path!")
  endif()
endif()

project(VitaMips)
include("${VITASDK}/share/vita.cmake" REQUIRED)
set(VITA_APP_NAME "VitaMips")
set(VITA_TITLEID  "VITAMIPS0")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wl,-q -Wall -O3")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti -fno-exceptions")
set(VITA_MAKE_FSELF_FLAGS "${VITA_MAKE_FSELF_FLAGS} -a 0x2800000000000013")

# Builds
FUNCTION(ADD_RESOURCES out_var)
  SET(result)
  FOREACH(ref_f ${ARGN})
  if (IS_ABSOLUTE "${ref_f}")
    SET(out_f "${ref_f}.o")
    STRING(REPLACE "${CMAKE_CURRENT_BINARY_DIR}/" "" in_f "${ref_f}")
    SET(work_dir "${CMAKE_CURRENT_BINARY_DIR}")
  else()
    SET(out_f "${CMAKE_CURRENT_BINARY_DIR}/${ref_f}.o")
    SET(in_f "${ref_f}")
    SET(work_dir "${CMAKE_SOURCE_DIR}")
  endif()
  GET_FILENAME_COMPONENT(out_dir ${out_f} DIRECTORY)
  ADD_CUSTOM_COMMAND(OUTPUT ${out_f}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${out_dir}
    COMMAND ${CMAKE_LINKER} -r -b binary -o ${out_f} ${in_f}
    DEPENDS ${ref_f}
    WORKING_DIRECTORY ${work_dir}
    COMMENT "Building resource ${out_f}"
    VERBATIM
    )
  LIST(APPEND result ${out_f})
  ENDFOREACH()
  SET(${out_var} "${result}" PARENT_SCOPE)
ENDFUNCTION()

file(GLOB res_files RELATIVE
  ${CMAKE_SOURCE_DIR}
  res/payload.bin
  res/license.rif
)

add_resources(vitamips_res
  ${res_files}
)

add_executable(VitaMips
  main.c
  scr_printf.c
  font.c
  ${vitamips_res}
)

target_link_libraries(VitaMips
  SceCompat_stub
  SceCtrl_stub
  SceDisplay_stub
)

vita_create_self(eboot.bin VitaMips UNSAFE)
vita_create_vpk(VitaMips.vpk ${VITA_TITLEID} eboot.bin
  VERSION ${VITA_VERSION}
  NAME ${VITA_APP_NAME}
)

add_custom_target(copy
  COMMAND cp eboot.bin F:/app/${VITA_TITLEID}/eboot.bin
  DEPENDS eboot.bin
)
