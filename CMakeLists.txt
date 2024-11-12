# ======================================
#  @brief  DAO Build Script
#  @author Thomas Davies
#  @date   November 2024
# ======================================

cmake_minimum_required(VERSION 3.13)

# =============================== #
#      Generate Build System
# =============================== #
project(DAO)

# DAO sub-directory paths.
set(DAOIncludes "${CMAKE_SOURCE_DIR}/include")
set(ProtobuffDir "${CMAKE_SOURCE_DIR}/proto")
set(BuildDir "${CMAKE_BINARY_DIR}")

# Declare installation destinations.
set(INCLUDE "include")
set(PYTHON "python")
set(JULIA "julia")
set(LIB64 "lib64")
set(DATA "data")
set(BIN "bin")

# Ensure installation destinations exist.
file(MAKE_DIRECTORY ${INCLUDE})
file(MAKE_DIRECTORY ${PYTHON})
file(MAKE_DIRECTORY ${JULIA})
file(MAKE_DIRECTORY ${LIB64})
file(MAKE_DIRECTORY ${DATA})
file(MAKE_DIRECTORY ${BIN})

# Invoke subdirectory cmake scripts.
add_subdirectory("src")
add_subdirectory("proto")

# Install pre-existing DAO project files.
file(GLOB daoHeaders "include/*.h" "include/*.hpp")
install(FILES ${daoHeaders} DESTINATION ${INCLUDE})

file(GLOB daoPy "src/python/*.py")
install(FILES ${daoPy} DESTINATION ${PYTHON})

file(GLOB daoJl "src/julia/*.jl")
install(FILES ${daoJl} DESTINATION ${JULIA})

file(GLOB daoScr "scripts/*")
install(FILES ${daoScr} DESTINATION ${BIN})

file(GLOB daoLogo "logo/*.png")
install(FILES ${daoLogo} DESTINATION ${DATA})
# =============================== #

