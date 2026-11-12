cmake_minimum_required(VERSION 3.25)
project(ReJuiced CXX C)

set(X86RECOMP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vendor/x86recomp" CACHE PATH "x86recomp checkout (tools/fetch-x86recomp.sh)")
set(CMAKE_LINK_DEPENDS_NO_SHARED ON)
add_subdirectory("${X86RECOMP_DIR}" x86recomp)
find_package(SDL3 REQUIRED CONFIG)

set(JUICED_GAME_DIR "" CACHE PATH "The installed game (its data files)")
set(JUICED_EXE "${CMAKE_CURRENT_SOURCE_DIR}/work/Juiced.exe" CACHE FILEPATH "The prepared EXE (tools/prepare-exe.sh)")
set(JUICED_GENERATED "${CMAKE_CURRENT_SOURCE_DIR}/generated" CACHE PATH "The lifter's output (tools/lift.sh)")

if(NOT EXISTS "${JUICED_GENERATED}/table.cpp")
  message(FATAL_ERROR "no lifted code in ${JUICED_GENERATED}: run tools/lift.sh first")
endif()
recomp_add_lifted(juiced_lifted "${JUICED_GENERATED}")
add_executable(juiced src/main.cpp src/hooks.cpp $<TARGET_OBJECTS:juiced_lifted>)
target_compile_features(juiced PRIVATE cxx_std_20)
target_include_directories(juiced PRIVATE "${X86RECOMP_DIR}/src")
target_compile_definitions(juiced PRIVATE
  JUICED_DEFAULT_GAME_DIR="${JUICED_GAME_DIR}"
  JUICED_DEFAULT_EXE="${JUICED_EXE}"
  JUICED_ASSETS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets"
  JUICED_PORT_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(juiced PRIVATE recomp_runtime recomp_d3d9 recomp_dsound recomp_quartz recomp_bink
                      recomp_dinput8 recomp_xinput SDL3::SDL3)
