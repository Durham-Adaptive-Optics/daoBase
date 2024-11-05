# ======================================
#  @brief  DAO Build Script
#  @author Thomas Davies
#  @date   2024-11-05 
#**********************************************/
# ======================================

# ------------------------------------------
#                Preamble
# ------------------------------------------
cmake_minimum_required(VERSION 3.4) # todo: require what min version?
include_directories("${CMAKE_SOURCE_DIR}/include")
project(DAO)

# ------------------------------------------
#          daoNuma shared-library
# ------------------------------------------
function(daoNuma)
    set(compileFlags,
        -Ofast 
        -Wall 
        -Wextra 
        # -march=core-avx2 
        # -mavx2
    )

    set(linkFlags -lnuma) 

    set(sources "${CMAKE_SOURCE_DIR}/src/cpp/daoNuma.cpp")

    # ----

    add_library(daoNuma SHARED ${sources})
    target_compile_options(daoNuma PRIVATE ${compileFlags})
    target_link_options(daoNuma PRIVATE ${linkFlags})
endfunction()

# ------------------------------------------
#           dao shared-library
# ------------------------------------------
function(dao)
    set(compileFlags,
        # Common flags
        -Ofast 
        -Wall
        -Wextra 
        -pipe
        -fdiagnostics-color=auto 
        -fstack-protector
        
        # Platform-specific flags
        $<$<PLATFORM_ID:Darwin>: -std=c++17>
        $<$<OR:$<PLATFORM_ID:Windows>,$<PLATFORM_ID:Linux>>: -lrt>

        # Build-specific flags
        $<$<CONFIG:ADDSAN>: -g -sanitize=address>
        $<$<CONFIG:DEBUG>: -g>
    )

    set(linkFlags
        # Common flags

        # Platform-specific flags
        $<$<PLATFORM_ID:Darwin>: -L/opt/homebrew/lib>
        $<$<OR:$<PLATFORM_ID:Windows>,$<PLATFORM_ID:Linux>>: -lnuma>

        # Build-specific flags
        $<$<CONFIG:ADDSAN>: -sanitize=address>
    ) 

    set(sources "${CMAKE_SOURCE_DIR}/src/c/dao.c")

    # ----

    add_library(dao SHARED ${sources})
    target_compile_options(dao PRIVATE ${compileFlags})
    target_link_options(dao PRIVATE ${linkFlags})
endfunction()

# ------------------------------------------
#                Build
# ------------------------------------------
daoNuma()
dao()
