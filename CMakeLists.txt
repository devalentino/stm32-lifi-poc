#############################################################################################################################
# file:  CMakeLists.txt
# brief: Template "CMakeLists.txt" for building of executables and static libraries.
#
# usage: Edit "VARIABLES"-section to suit project requirements.
#        For debug build:
#          cmake -DCMAKE_TOOLCHAIN_FILE=cubeide-gcc.cmake  -S ./ -B Debug -G"Unix Makefiles" -DCMAKE_BUILD_TYPE=Debug
#          make -C Debug VERBOSE=1 -j
#        For release build:
#          cmake -DCMAKE_TOOLCHAIN_FILE=cubeide-gcc.cmake  -S ./ -B Release -G"Unix Makefiles" -DCMAKE_BUILD_TYPE=Release
#          make -C Release VERBOSE=1 -j
#############################################################################################################################
cmake_minimum_required(VERSION 3.20)

option(LIFI_BUILD_TESTS "Build host-side unit tests" OFF)

  if(LIFI_BUILD_TESTS)
    project(lifi_tests C)
    enable_testing()

    add_executable(test_integration
      Tests/integration/test_runner.c
      Tests/integration/test_lifi_protocol.c
      Tests/integration/test_lifi_modem.c
      Tests/fixtures/lifi_socket_fixture.c
      Tests/fixtures/lifi_modem_fixture.c
      Tests/fake/fake_lifi_callbacks.c
      Tests/fake/fake_lifi_transport.c
      Tests/fake/fake_lifi_transmitter.c
      Tests/fake/fake_lifi_receiver.c
      Sources/lifi_host_interface.c
      Sources/lifi_modem.c
      Sources/lifi_protocol/lifi_protocol.c
      Sources/lifi_protocol/lifi_protocol_utils.c
      Sources/lifi_protocol/lifi_protocol_tx.c
      Sources/lifi_protocol/lifi_protocol_rx.c
      Sources/lifi_protocol/lifi_protocol_router.c
      External/unity/src/unity.c
    )

    target_include_directories(test_integration PRIVATE
      Tests/fake
      Tests/fixtures
      Sources
      Sources/lifi_protocol
      External/unity/src
      External/fff
    )

    add_test(NAME test_integration COMMAND test_integration)
    return()
  endif()

###################### CONSTANTS ######################################
set (PROJECT_TYPE_EXECUTABLE          "exe")
set (PROJECT_TYPE_STATIC_LIBRARY      "static-lib")
set (MCPU_CORTEX_M0				      "-mcpu=cortex-m0")
set (MCPU_CORTEX_M0PLUS				  "-mcpu=cortex-m0plus")
set (MCPU_CORTEX_M3				      "-mcpu=cortex-m3")
set (MCPU_CORTEX_M4				      "-mcpu=cortex-m4")
set (MCPU_CORTEX_M7				      "-mcpu=cortex-m7")
set (MCPU_CORTEX_M33				  "-mcpu=cortex-m33")
set (MCPU_CORTEX_M55				  "-mcpu=cortex-m55")
set (MCPU_CORTEX_M85				  "-mcpu=cortex-m85")
set (MFPU_FPV4_SP_D16                 "-mfpu=fpv4-sp-d16")
set (MFPU_FPV4                        "-mfpu=vfpv4")
set (MFPU_FPV5_D16                    "-mfpu=fpv5-d16")
set (RUNTIME_LIBRARY_REDUCED_C        "--specs=nano.specs")
set (RUNTIME_LIBRARY_STD_C            "")
set (RUNTIME_LIBRARY_SYSCALLS_MINIMAL "--specs=nosys.specs")
set (RUNTIME_LIBRARY_SYSCALLS_NONE    "")
set (MFLOAT_ABI_SOFTWARE              "-mfloat-abi=soft")
set (MFLOAT_ABI_HARDWARE              "-mfloat-abi=hard")
set (MFLOAT_ABI_MIX                   "-mfloat-abi=softfp")
#######################################################################

###################### VARIABLES ######################################
set (PROJECT_NAME             "lifi")
set (PROJECT_TYPE             "exe")
set (LINKER_SCRIPT            "../STM32F411RETX_FLASH.ld")
set (MCPU                     "-mcpu=Cortex-M4")
set (MFPU                 "-mfpu=fpv4-sp-d16")
set (MFLOAT_ABI               "-mfloat-abi=soft")
set (RUNTIME_LIBRARY          "--specs=nano.specs")
set (RUNTIME_LIBRARY_SYSCALLS "--specs=nosys.specs")


set (PROJECT_SOURCES
	# LIST SOURCE FILES HERE
	Startup/startup_stm32f411retx.s
	Sources/lifi_transmitter.c
	Sources/lifi_receiver.c
	Sources/lifi_protocol/lifi_protocol.c
	Sources/lifi_protocol/lifi_protocol_utils.c
	Sources/lifi_protocol/lifi_protocol_tx.c
	Sources/lifi_protocol/lifi_protocol_rx.c
	Sources/lifi_protocol/lifi_protocol_router.c
	Sources/lifi_host_interface.c
	Sources/lifi_modem.c
	Sources/main.c
	Sources/syscalls.c
	Sources/sysmem.c
	
	Sources/system_stm32f4xx.c
	Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal.c
	Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_cortex.c
	Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_dma.c
	  
	Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_gpio.c
	Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_rcc.c
	Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_rcc_ex.c
	Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_tim.c
	Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_tim_ex.c
	Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_uart.c
	)

set (PROJECT_DEFINES
	# LIST COMPILER DEFINITIONS HERE
	STM32F411xE
	USE_HAL_DRIVER

    )

set (PROJECT_INCLUDES
	# LIST INCLUDE DIRECTORIES HERE
	Sources
	Sources/lifi_protocol
    Drivers/CMSIS/Include
    Drivers/CMSIS/Device/ST/STM32F4xx/Include
    Drivers/STM32F4xx_HAL_Driver/Inc

    )

############ MODIFY ACCORDING TO REQUIREMENTS) ########################

#######################################################################

################## PROJECT SETUP ######################################
project(${PROJECT_NAME})
enable_language(ASM)

if (${PROJECT_TYPE} MATCHES ${PROJECT_TYPE_EXECUTABLE})
  add_executable(${PROJECT_NAME} ${PROJECT_SOURCES})
  add_custom_command(TARGET ${CMAKE_PROJECT_NAME} POST_BUILD COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${CMAKE_PROJECT_NAME}>)
elseif (${PROJECT_TYPE} MATCHES ${PROJECT_TYPE_STATIC_LIBRARY})
  add_library(${PROJECT_NAME} ${PROJECT_SOURCES})
endif()

add_compile_definitions (${PROJECT_DEFINES})
include_directories (${PROJECT_INCLUDES})

set (CMAKE_EXECUTABLE_SUFFIX ".elf")
set (CMAKE_STATIC_LIBRARY_SUFFIX ".a")
set (CMAKE_C_FLAGS "${MCPU} -std=gnu11 ${MFPU} ${MFLOAT_ABI} ${RUNTIME_LIBRARY} -mthumb -Wall -Werror")
set (CMAKE_EXE_LINKER_FLAGS "-T${LINKER_SCRIPT} ${RUNTIME_LIBRARY_SYSCALLS} -Wl,-Map=test.map -Wl,--gc-sections -static -Wl,--start-group -lc -lm -Wl,--end-group")
set (CMAKE_ASM_FLAGS "${CMAKE_C_FLAGS} -x assembler-with-cpp")
