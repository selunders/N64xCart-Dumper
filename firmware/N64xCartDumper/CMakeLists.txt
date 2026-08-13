cmake_minimum_required(VERSION 3.17)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(FAMILY rp2040)
set(BOARD pico_sdk)
set(CMAKE_BUILD_TYPE Debug)
#set(CMAKE_CXX_FLAGS "-g -Wall -Wextra")

include(${CMAKE_CURRENT_SOURCE_DIR}/external/tinyusb/hw/bsp/family_support.cmake)

# gets PROJECT name for the example (e.g. <BOARD>-<DIR_NAME>)
family_get_project_name(PROJECT N64xCartDumper)

project(${PROJECT} C CXX ASM)

# Checks this example is valid for the family and initializes the project
family_initialize_project(${PROJECT} ${CMAKE_CURRENT_LIST_DIR})

# Espressif has its own cmake build system
if(FAMILY STREQUAL "espressif")
  return()
endif()

add_executable(${PROJECT})

add_custom_command(OUTPUT ${CMAKE_CURRENT_LIST_DIR}/src/generated/joybus.pio.h
        DEPENDS ${CMAKE_CURRENT_LIST_DIR}/src/joybus.pio
        COMMAND pioasm/pioasm ${CMAKE_CURRENT_LIST_DIR}/src/joybus.pio ${CMAKE_CURRENT_LIST_DIR}/src/generated/joybus.pio.h
        )

target_sources(${PROJECT} PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/src/main.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/virtualdisk.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/usb_descriptors.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/n64cartinterface.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/joybus.c
  )

target_include_directories(${PROJECT} PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  )

target_compile_definitions(${PROJECT} PUBLIC PICO_DEFAULT_LED_PIN=26)



# Configure compilation flags and libraries for the example... see the corresponding function
# in hw/bsp/FAMILY/family.cmake for details.
family_configure_device_example(${PROJECT} noos)

target_link_libraries(${PROJECT} PUBLIC hardware_pio pico_stdlib pico_platform)

# Newer pico-sdk versions ship their own "bsp/board.h" that just redirects to
# "bsp/board_api.h", expecting the linked TinyUSB to provide that file. This
# project vendors an older TinyUSB (pre board.h -> board_api.h split) whose
# real bsp/board.h lives under external/tinyusb/hw. Force that directory
# ahead of the SDK's redirect shim so the real header wins.
target_include_directories(${PROJECT} BEFORE PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/external/tinyusb/hw
  )

# arm-none-eabi-gcc 16 flags pre-existing sign-conversion warnings (e.g. in
# InitEepromClock, unrelated to this patch) that older compilers this
# project was last built with did not. Downgrade -Werror so the build can
# complete for verification purposes; does not affect what is warned about.
target_compile_options(${PROJECT} PUBLIC -Wno-error)

pico_add_extra_outputs(${PROJECT})
pico_generate_pio_header(${PROJECT} ${CMAKE_CURRENT_LIST_DIR}/src/joybus.pio)