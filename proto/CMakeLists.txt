# ================================================= 
#             daoProto Shared Library
# ================================================= 

set(pbSrcFiles # Base names of source Google Protobuf files.
    "daoCommand"
    "daoEvent"
    "daoLogging"
)

set(compileFlags -std=c++11)
set(linkFlags)

# ---

find_program(pbcompiler NAMES protoc REQUIRED)

# Tell CMake to regenerate the output bindings whenever the
# source .proto files change.
set(bindings)
foreach(pbFile IN LISTS pbSrcFiles)
    # Requires absolute paths for CMake file tracking ..
    set(binding "${BUILD_DIR}/${pbFile}.pb.cc")
    list(APPEND bindings ${binding})

    add_custom_command(
        OUTPUT ${binding} "${BUILD_DIR}/${pbFile}.pb.h" "${BUILD_DIR}/${pbFile}.py"
        DEPENDS "${SRCPROTO_DIR}/${pbFile}.proto"
        COMMAND ${pbcompiler}
        ARGS -I${SRCPROTO_DIR} --cpp_out=${BUILD_DIR} --python_out=${BUILD_DIR} "${pbFile}.proto"
    )
endforeach()

# ---

add_library(daoProto SHARED ${bindings})
target_compile_options(daoProto PRIVATE ${compileFlags})
target_link_options(daoProto PRIVATE ${linkFlags})
# ================================================= 
