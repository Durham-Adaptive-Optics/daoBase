# ================================================= 
#              daoNuma Shared Library
# ================================================= 
set(sources "${SRC_DIR}/cpp/daoNuma.cpp")

set(compileFlags
    -Ofast
    -Wall
    -Wextra
    -march=core-avx2
    -mavx2
)

set(linkFlags
    -lnuma
    -Bstatic
    -Bdynamic
)

add_library(daoNuma SHARED ${sources})
set_target_properties(daoNuma PROPERTIES SOVERSION 0)
target_compile_options(daoNuma PRIVATE ${compileFlags})
target_link_options(daoNuma PRIVATE ${linkFlags})
# ================================================= 

# ================================================= 
#             dao Shared Library
# ================================================= 
set(sources "${SRC_DIR}/c/dao.c")

set(compileFlags
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

add_library(dao SHARED ${sources})
target_compile_options(dao PRIVATE ${compileFlags})
target_link_options(dao PRIVATE ${linkFlags})
# ================================================= 
