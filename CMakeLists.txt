cmake_minimum_required(VERSION 3.0)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  if(DEFINED ENV{VITASDK})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake" CACHE PATH "toolchain file")
  else()
    message(FATAL_ERROR "Please define VITASDK to point to your SDK path!")
  endif()
endif()

project(psp2waveviewer)
include("${VITASDK}/share/vita.cmake" REQUIRED)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wl,-q -Wall -O2 -fno-inline -fno-builtin -fshort-wchar")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti -fno-exceptions -Wl,-q -Wall -O2 -fno-inline -fno-builtin -fshort-wchar")

set(VITA_MAKE_FSELF_FLAGS "${VITA_MAKE_FSELF_FLAGS} -a 0x2F00000000000005")

enable_language(C ASM)

add_subdirectory(libpaf)

add_executable(${PROJECT_NAME}
  src/main.c
)

add_dependencies(${PROJECT_NAME} ScePaf_stub_weak)

target_link_libraries(${PROJECT_NAME}
  SceKernelThreadMgr_stub
  SceSysmodule_stub
  SceCtrl_stub
  ScePaf_stub
  SceLibKernel_stub
  SceIofilemgr_stub
  SceProcessmgr_stub
  ${CMAKE_CURRENT_BINARY_DIR}/libpaf/libScePaf_stub_weak.a
)

set_target_properties(${PROJECT_NAME}
  PROPERTIES LINK_FLAGS "-nostdlib"
)

vita_create_self(${PROJECT_NAME}.self ${PROJECT_NAME}
  CONFIG exports.yml
  UNSAFE
  STRIPPED
  REL_OPTIMIZE
)

vita_create_vpk(psp2waveviewer.vpk "WAVE00001" ${PROJECT_NAME}.self
  VERSION "01.00"
  NAME "Wave Viewer"
)

vita_create_stubs(stubs_${PROJECT_NAME} ${PROJECT_NAME} exports.yml)
