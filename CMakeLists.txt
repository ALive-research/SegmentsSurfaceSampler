cmake_minimum_required(VERSION 3.15)

project(SegmentSurfaceSampler)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/CMake/Modules")

if (NOT DEFINED TCLAP_INCLUDE_PATH)
  set(TCLAP_INCLUDE_PATH "${CMAKE_CURRENT_LIST_DIR}/external/tclap/include")
endif()

find_package(ITK REQUIRED)
find_package(VTK REQUIRED)
find_package(TCLAP REQUIRED)

include(${ITK_USE_FILE})

add_executable(${PROJECT_NAME})
target_sources(${PROJECT_NAME} PRIVATE main.cxx)
target_include_directories(${PROJECT_NAME} PRIVATE ${TCLAP_INCLUDE_DIR})
target_link_libraries(${PROJECT_NAME} PRIVATE ${ITK_LIBRARIES} ${VTK_LIBRARIES})
