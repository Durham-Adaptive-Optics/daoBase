#!/usr/bin/env julia
#
# Small runnable example for dao.jl.
#
# Run with:
#     julia example_basic.jl
# (needs libdao.so on your library path, e.g. via daoBase's dao_env.sh)

include(joinpath(@__DIR__, "dao.jl"))

name = "/tmp/test_julia.im.shm"

# Create a new 4x4 Float32 SHM (or overwrite an existing one).
writer = dao.shm(name, ones(Float32, 4, 4))
println("created $name, counter = $(dao.get_counter(writer))")

dao.set_data(writer, fill(2.0f0, 4, 4))
println("wrote a frame, counter = $(dao.get_counter(writer))")

# Attach to the same SHM from a second handle, as a separate process
# reading it would.
reader = dao.shm(name)
data = dao.get_data(reader)
println("read back size=$(size(data)), sum=$(sum(data))")

dao.close(writer)
dao.close(reader)
