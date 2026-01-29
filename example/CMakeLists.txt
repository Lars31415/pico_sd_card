cmake_minimum_required(VERSION 3.30.0)

# Pull in SDK (must be before project)
include(../../pico_sdk_import.cmake)
include(../../pico_extras_import_optional.cmake)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

if(PICO_SDK_VERSION_STRING VERSION_LESS "1.5.1")
  message(
    FATAL_ERROR
    "Raspberry Pi Pico SDK version 2.2.0 (or later) required. Your version is ${PICO_SDK_VERSION_STRING}"
  )
endif()
message("PICO_SDK_VERSION_STRING: " ${PICO_SDK_VERSION_STRING})

set(PROGRAM_NAME test)

project(${PROGRAM_NAME} VERSION 0.1.0 LANGUAGES C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

add_executable(${PROGRAM_NAME}
  main.cpp
  ../hly_sd_card.c
  ${GITHUB_ROOT_DIR}/utils/crc16.c
  ${GITHUB_ROOT_DIR}/utils/crc7.c
)

target_compile_definitions(${PROGRAM_NAME} PUBLIC
)

target_sources(${PROGRAM_NAME} PRIVATE
)

target_link_libraries(${PROGRAM_NAME}
  pico_stdlib
  hardware_spi
)

target_compile_options(${PROGRAM_NAME} PUBLIC)

target_include_directories(${PROGRAM_NAME} PUBLIC
  ../
  ${GITHUB_ROOT_DIR}/utils
)

set_target_properties(${PROGRAM_NAME} PROPERTIES SUFFIX ".elf")

pico_enable_stdio_usb(${PROGRAM_NAME} 1)
pico_enable_stdio_uart(${PROGRAM_NAME} 1)

pico_add_extra_outputs(${PROGRAM_NAME})
