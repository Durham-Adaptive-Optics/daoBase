# ======================================
#  @brief  DAO Build Script
#  @author Thomas Davies
#  @date   November 2024
# ======================================

function(daoProto)
    set(sources
        "daoCommand"
        "daoEvent"
        "daoLogging"
    )

    set(compileFlags
        $<$<CONFIG:DEBUG>: -O0>
        $<$<CONFIG:ASAN>: -g -fsanitize=address>
        -std=c++11
    )

    set(linkFlags
        # put linker flags here
    )

    find_program(protobuffCompiler NAMES protoc REQUIRED)

    set(cxxFiles)
    foreach(file IN LISTS sources)
        set(cxx "${BuildDir}/${file}.pb.cc")
        set(hpp "${BuildDir}/${file}.pb.h")
        set(py "${BuildDir}/${file}_pb2.py")
        list(APPEND cxxFiles ${cxx})

        add_custom_command(
            OUTPUT ${cxx} ${hpp} ${py}
            DEPENDS "${ProtobuffDir}/${file}.proto"
            COMMAND ${protobuffCompiler}
            ARGS -I${ProtobuffDir} --cpp_out=${BuildDir} --python_out=${BuildDir} "${file}.proto"
            COMMENT "Generating C++/Python bindings for ${file}.proto"
        )

        install(FILES ${hpp} DESTINATION ${INCLUDE})
        install(FILES ${py} DESTINATION ${PYTHON})
    endforeach()

    add_library(daoProto SHARED ${cxxFiles})
    target_compile_options(daoProto PRIVATE ${compileFlags})
    target_link_options(daoProto PRIVATE ${linkFlags})
    install(TARGETS daoProto DESTINATION ${LIB64})
endfunction(daoProto)

# =============================== #

daoProto()

