cmake_minimum_required(VERSION 3.19)

if (${TOOLCHAIN_LANGUAGE} MATCHES "MikroC")
    project(R7FA4M2_Multi_Click_Smartwatch LANGUAGES MikroC)
else()
    project(R7FA4M2_Multi_Click_Smartwatch LANGUAGES C ASM)
endif()

include(mikroeUtils)

if (${MIKROSDK_TYPE} STREQUAL "legacy")
    legacy_cmake_path()
endif()

set(PROJECT_TYPE "standard" CACHE STRING "" FORCE)

add_executable(R7FA4M2_Multi_Click_Smartwatch
    src/app_controller.c
    src/app_interrupts.c
    src/bletiny.c
    src/iic0_bus.c
    src/ips2_display.c
    src/main.c
    src/rtc14.c
    src/temphum27.c

    include/app_config.h
    include/app_controller.h
    include/app_interrupts.h
    include/app_types.h
    include/bletiny.h
    include/iic0_bus.h
    include/ips2_display.h
    include/project_platform.h
    include/ra4m2_baremetal.h
    include/rtc14.h
    include/temphum27.h
)

target_include_directories(R7FA4M2_Multi_Click_Smartwatch PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include/
)

find_package(MikroC.Core REQUIRED)
target_link_libraries(R7FA4M2_Multi_Click_Smartwatch
    PUBLIC
        MikroC.Core
)

if (${MIKROSDK_TYPE} STREQUAL "legacy")
    find_package(MikroC.System REQUIRED)
    target_link_libraries(R7FA4M2_Multi_Click_Smartwatch
        PUBLIC
            MikroC.System
    )
endif()

if(COMPILER_REVISION)
    if(${COMPILER_REVISION} VERSION_GREATER_EQUAL "1.0")
        if(${TOOLCHAIN_LANGUAGE} STREQUAL "GNU")
            if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/.meproject)
                fetch_startup_name(${CMAKE_CURRENT_SOURCE_DIR}/.meproject startupName)
                if(startupName)
                    add_startup_all_targets(. ${CMAKE_CURRENT_SOURCE_DIR} ${startupName})
                endif()
                fetch_linker_name(${CMAKE_CURRENT_SOURCE_DIR}/.meproject linkerName)
                if(linkerName)
                    add_ld_all_targets(. ${CMAKE_CURRENT_SOURCE_DIR} ${linkerName})
                endif()
            endif()
        endif()
    endif()
endif()
