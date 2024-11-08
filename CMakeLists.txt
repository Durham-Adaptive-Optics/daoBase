# ======================================
#  @brief  DAO Build Script
#  @author Thomas Davies
#  @date   November 2024
# ======================================

# ================================================= 
cmake_minimum_required(VERSION 3.4)
project(DAO)
# ================================================= 

# ================================================= 
# Ensure the user has specified where DAO should 
# be installed.
# ================================================= 
if(NOT DEFINED InstallDir)
    message(FATAL_ERROR "Please specify the DAO installation directory via the cmake switch -DInstallDir=<path>")
endif()
# ================================================= 

# ================================================= 
#               Required Directories
# ================================================= 
set(BUILD_DIR "${CMAKE_SOURCE_DIR}/build") # Where build files are generated.
set(SRC_DIR "${CMAKE_SOURCE_DIR}/src") # Location of source code.
set(SRCINCLUDE_DIR "${CMAKE_SOURCE_DIR}/include")
set(SRCPROTO_DIR "${CMAKE_SOURCE_DIR}/proto")

set(INCLUDE_DIR "${InstallDir}/include")
set(PYTHON_DIR "${InstallDir}/python")
set(JULIA_DIR "${InstallDir}/julia")
set(BIN_DIR "${InstallDir}/bin")
set(LIB64_DIR "${InstallDir}/lib64")

file(MAKE_DIRECTORY ${INCLUDE_DIR})
file(MAKE_DIRECTORY ${PYTHON_DIR})
file(MAKE_DIRECTORY ${JULIA_DIR})
file(MAKE_DIRECTORY ${BIN_DIR})
file(MAKE_DIRECTORY ${LIB64_DIR})
# ================================================= 

# ================================================= 
#                      BUILD
# ================================================= 
include_directories(${SRCINCLUDE_DIR}) # Tell compiler where (protobuff, dao) includes are.
add_subdirectory("src")
add_subdirectory("proto")

# ================================================= 


# ================================================= 
#                   INSTALLATION
# ================================================= 
file(GLOB pbHxx "*.pb.h") # protobuff
file(GLOB pbPy "*.py") # protobuff
file(GLOB daoHxx "include/*.h" "include/*.hpp")
file(GLOB pyScr "src/python/*.py") # python scripts
file(GLOB julScr "src/julia/*.jl") # julia scripts
file(GLOB scr "scripts/*") # Misc scripts
install(FILES ${pbHxx} DESTINATION ${INCLUDE_DIR})
install(FILES ${daoHxx} DESTINATION ${INCLUDE_DIR})
install(FILES ${pyScr} DESTINATION ${PYTHON_DIR})
install(FILES ${scr} DESTINATION ${PYTHON_DIR})
install(FILES ${julScr} DESTINATION ${JULIA_DIR})
install(FILES ${pbPy} DESTINATION ${BIN_DIR})
install(TARGETS daoProto DESTINATION ${LIB64_DIR})
install(TARGETS daoNuma DESTINATION ${LIB64_DIR})
install(TARGETS dao DESTINATION ${LIB64_DIR})

# ================================================= 
