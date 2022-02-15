include("./.env.cmake" RESULT_VARIABLE LOCAL_ENV)
message(STATUS "Local .env.cmake: ${LOCAL_ENV}")

cmake_minimum_required(VERSION 3.11.0)

set(NAME LeoEngine)

set(SHADERS_PATH "${CMAKE_SOURCE_DIR}/resources/shaders")
 
message(STATUS "Using generator ${CMAKE_GENERATOR}")
if (CMAKE_GENERATOR STREQUAL "MinGW Makefiles")
    if (NOT MINGW_PATH)
      message(FATAL_ERROR "MINGW_PATH not set in .env.cmake")
    endif()
    set(USE_MINGW "True")
    set(CMAKE_C_COMPILER ${MINGW_PATH}/bin/gcc.exe)
    set(CMAKE_CXX_COMPILER  ${MINGW_PATH}/bin/g++.exe)
endif()

project(${NAME} VERSION 1.0.0)
 
if (DEFINED VULKAN_INCLUDE_PATH AND DEFINED VULKAN_LIB_PATH)
  set(VULKAN_FOUND "True")
else()
  find_package(Vulkan REQUIRED)
  set (VULKAN_INCLUDE_PATH "$ENV{VULKAN_SDK}/Include")
  set (VULKAN_LIB_PATH "$ENV{VULKAN_SDK}/Lib")
  set(VULKAN_FOUND "True")
endif()
if (NOT VULKAN_FOUND)
    message(FATAL_ERROR "Could not find Vulkan library!")
else()
    message(STATUS "Using Vulkan lib at: ${VULKAN_LIB_PATH}")
endif()

if (NOT DEFINED GLFW_LIB_PATH OR NOT DEFINED GLFW_INCLUDE_PATH)
  message(FATAL_ERROR "GLFW_LIB_PATH and GLFW_INCLUDE_PATH are not set in .env.cmake!")
endif()

if (NOT DEFINED ASSIMP_INCLUDE_PATH OR NOT DEFINED ASSIMP_LIB_PATH)
  message(FATAL_ERROR "ASSIMP_LIB_PATH and ASSIMP_INCLUDE_PATH is not set in .env.cmake!")
endif()

if (NOT DEFINED VMA_INCLUDE_PATH OR NOT DEFINED VMA_LIB_PATH)
  message(FATAL_ERROR "VMA_LIB_PATH and VMA_INCLUDE_PATH is not set in .env.cmake!")
endif()

if (NOT DEFINED GLAD_LIB_PATH OR NOT DEFINED GLAD_INCLUDE_PATH)
  message(FATAL_ERROR "GLAD_LIB_PATH and GLAD_INCLUDE_PATH are not set in .env.cmake!")
endif()

include_directories(${INCLUDE_PATH})
message("Additional include directory: ${INCLUDE_PATH}")

include_directories(src)

file(GLOB_RECURSE SOURCES ${PROJECT_SOURCE_DIR}/src/*.cpp ${PROJECT_SOURCE_DIR}/external/*.cpp ${PROJECT_SOURCE_DIR}/external/*.c)

add_executable(${PROJECT_NAME} ${SOURCES})
 
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_17)

set_target_properties(${PROJECT_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}")

message(STATUS "CREATING BUILD FOR WINDOWS")

if (USE_MINGW)
  target_include_directories(${PROJECT_NAME} PUBLIC
    ${MINGW_PATH}/include
  )
  target_link_directories(${PROJECT_NAME} PUBLIC
    ${MINGW_PATH}/lib
  )
endif()

target_include_directories(${PROJECT_NAME} PUBLIC
  ${PROJECT_SOURCE_DIR}/src
  ${VULKAN_INCLUDE_PATH}
  ${GLFW_INCLUDE_PATH}
  ${GLAD_INCLUDE_PATH}
  ${ASSIMP_INCLUDE_PATH}
  ${VMA_INCLUDE_PATH}
  ${INCLUDE_PATH}
  )

target_link_directories(${PROJECT_NAME} PUBLIC
  ${VULKAN_LIB_PATH}
  ${GLFW_LIB_PATH}
  ${ASSIMP_LIB_PATH}
  ${VMA_LIB_PATH}
)

target_link_libraries(${PROJECT_NAME} glfw3 vulkan-1 assimp-vc140-mt VulkanMemoryAllocator)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${PROJECT_SOURCE_DIR}/external/bin"
        $<TARGET_FILE_DIR:${PROJECT_NAME}>)
