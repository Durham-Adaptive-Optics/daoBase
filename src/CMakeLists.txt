# ======================================
#  @brief  DAO Build Script
#  @author Thomas Davies
#  @date   November 2024
# ======================================

function(daoNuma)
    if(NOT WIN32)
        set(compilerFlags
            -Ofast
            -Wall
            -Wextra
            -march=core-avx2
            -mavx2

            $<$<CONFIG:DEBUG>: -O0>
            $<$<CONFIG:ASAN>: -g -fsanitize=address>
        )

        set(linkerFlags -lnuma)

        set(sources "cpp/daoNuma.cpp")

        add_library(daoNuma SHARED ${sources})
        target_include_directories(daoNuma PRIVATE ${DAOIncludes})
        target_compile_options(daoNuma PRIVATE ${compilerFlags})
        target_link_options(daoNuma PRIVATE ${linkerFlags})
        install(TARGETS daoNuma DESTINATION ${LIB64})
    endif()
endfunction(daoNuma)

function(dao)
    set(sources "c/dao.c")

    add_library(dao SHARED ${sources})
    target_include_directories(dao PRIVATE ${DAOIncludes})

    set(compilerFlags "")
    set(linkerFlags "")

    if(WIN32)
        list(APPEND compilerFlags -D_CRT_SECURE_NO_WARNINGS)
        list(APPEND compilerFlags -W3)
        list(APPEND compilerFlags $<$<CONFIG:DEBUG>: -Od>)
        list(APPEND compilerFlags $<$<CONFIG:ASAN>: -Zi -GS -analyze>)
        list(APPEND linkerFlags advapi32.lib)
    elseif(UNIX)
        list(APPEND compilerFlags -Ofast)
        list(APPEND compilerFlags -Wall)
        list(APPEND compilerFlags -Wextra)
        list(APPEND compilerFlags -pipe)
        list(APPEND compilerFlags -fdiagnostics-color=auto)
        list(APPEND compilerFlags -fstack-protector)
        list(APPEND compilerFlags -lrt)
        list(APPEND compilerFlags $<$<CONFIG:DEBUG>: -O0>)
        list(APPEND compilerFlags $<$<CONFIG:ASAN>: -g -fsanitize=address>)
        list(APPEND linkerFlags -lnuma)
    elseif(APPLE)
        list(APPEND compilerFlags -Ofast)
        list(APPEND compilerFlags -Wall)
        list(APPEND compilerFlags -Wextra)
        list(APPEND compilerFlags -pipe)
        list(APPEND compilerFlags -fdiagnostics-color=auto)
        list(APPEND compilerFlags -fstack-protector)
        list(APPEND compilerFlags $<$<CONFIG:DEBUG>: -O0>)
        list(APPEND compilerFlags $<$<CONFIG:ASAN>: -g -fsanitize=address)
        list(APPEND linkerFlags -L/opt/homebrew/lib)
    endif()

    target_compile_options(dao PRIVATE ${compilerFlags})
    target_link_options(dao PRIVATE ${linkerFlags})
    install(TARGETS dao DESTINATION ${LIB64})

endfunction(dao)

# =============================== #

daoNuma()
dao()

