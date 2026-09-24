module dao

export  daoShmOpen, daoShmSetData, daoShmSetDataPart, daoShmSetDataPartFinalize, daoShmCreate, daoShmWaitSem, daoShmWaitCounter, daoShmGetCounter, daoShmClose


# Basic type definitions for clarity and consistency
const uint8_t = UInt8
const int8_t = Int8
const uint16_t = UInt16
const int16_t = Int16
const uint32_t = UInt32
const int32_t = Int32
const uint64_t = UInt64
const int64_t = Int64
const float = Float32
const double = Float64

# Complex data types
struct complex_float
    re::float
    im::float
end

struct complex_double
    re::double
    im::double
end

# Keyword structure used in IMAGE, includes name, type, value, and a comment
struct IMAGE_KEYWORD
    name::NTuple{16, Cchar}    # keyword name
    type::Cchar                # data type: 'N': unused, 'L': long, 'D': double, 'S': 16-char string
    value::Union{int64_t, double, NTuple{16, Cchar}}  # value, using a union for different types
    comment::NTuple{80, Cchar} # comment field
end

# Structure for fixed-size timespec, ensuring 16-byte length
struct TIMESPECFIXED
    firstlong::int64_t
    secondlong::int64_t
end

# Image metadata structure, including various fields like name, size, data type, etc.
#
# This must match daoBase/include/dao.h's IMAGE_METADATA field-for-field
# (Linux/macOS layout; Windows adds no extra fields here). In particular
# lastNbArray is 2024 elements, not 512 - getting this wrong does not
# misalign the fields before it, but it does make sizeof(IMAGE_METADATA)
# wrong, which matters when indexing into a FIFO of metadata (image->md[i]).
@static if Sys.isapple()
    struct IMAGE_METADATA
        name::NTuple{80, Cchar}         # Image Name
        naxis::uint8_t                  # Number of axes (1, 2, or 3)
        size::NTuple{3, uint32_t}       # Size along each axis
        nelement::uint64_t              # Total number of elements
        atype::uint8_t                  # Data type
        creation_time::double           # Creation time since process start
        last_access::double             # Last access time since process start
        atime::Union{TIMESPECFIXED}     # Acquisition time, fixed size
        shared::uint8_t                 # 1 if in shared memory
        status::uint8_t                 # Image status (logging, etc.)
        logflag::uint8_t                # Logging flag
        sem::uint16_t                   # Number of semaphores in use
        cnt0::uint64_t                  # General purpose counter
        cnt1::uint64_t                  # In 3D buffer, last slice written
        cnt2::uint64_t                  # In event mode, number of events
        write::uint8_t                  # 1 if image is being written
        NBkw::uint16_t                  # Number of keywords
        lastPos::uint32_t               # Position of the last write
        lastNb::uint32_t                # Number of last write
        packetNb::uint32_t              # Packet number (for partial writes)
        packetTotal::uint32_t           # Total number of packets (for partial writes)
        lastNbArray::NTuple{2024, uint64_t}
        semCounter::NTuple{10, uint32_t}   # macOS-only: per-semaphore atomic counters
        semLogCounter::uint32_t            # macOS-only: log semaphore atomic counter
        fifo_size::uint32_t             # Number of slots in the FIFO
        fifo_last_written::uint32_t     # Index of the most recently written slot
    end
else
    struct IMAGE_METADATA
        name::NTuple{80, Cchar}         # Image Name
        naxis::uint8_t                  # Number of axes (1, 2, or 3)
        size::NTuple{3, uint32_t}       # Size along each axis
        nelement::uint64_t              # Total number of elements
        atype::uint8_t                  # Data type
        creation_time::double           # Creation time since process start
        last_access::double             # Last access time since process start
        atime::Union{TIMESPECFIXED}     # Acquisition time, fixed size
        shared::uint8_t                 # 1 if in shared memory
        status::uint8_t                 # Image status (logging, etc.)
        logflag::uint8_t                # Logging flag
        sem::uint16_t                   # Number of semaphores in use
        cnt0::uint64_t                  # General purpose counter
        cnt1::uint64_t                  # In 3D buffer, last slice written
        cnt2::uint64_t                  # In event mode, number of events
        write::uint8_t                  # 1 if image is being written
        NBkw::uint16_t                  # Number of keywords
        lastPos::uint32_t               # Position of the last write
        lastNb::uint32_t                # Number of last write
        packetNb::uint32_t              # Packet number (for partial writes)
        packetTotal::uint32_t           # Total number of packets (for partial writes)
        lastNbArray::NTuple{2024, uint64_t}
        fifo_size::uint32_t             # Number of slots in the FIFO
        fifo_last_written::uint32_t     # Index of the most recently written slot
    end
end

# The main IMAGE structure, including metadata, data array, and semaphores
#
# Linux/macOS layout (Windows uses wider handle types for shmfd/semlog/semptr
# and an extra shmfm field, and is not modeled here).
struct IMAGE
    name::NTuple{80, Cchar}    # Local name
    used::uint8_t              # Usage flag: 1 if used, 0 otherwise
    shmfd::int32_t             # File descriptor for shared memory
    memsize::uint64_t          # Total size in memory if shared
    semlog::Ptr{Cvoid}          # Pointer to semaphore for logging
    md::Ptr{IMAGE_METADATA}    # Pointer to IMAGE_METADATA
    array::Ptr{Cvoid}           # Pointer to data array, type-agnostic
    semptr::Ptr{Ptr{Cvoid}}     # Array of pointers to semaphores
    kw::Ptr{IMAGE_KEYWORD}     # Pointer to IMAGE_KEYWORD array
    semReadPID::Ptr{int32_t}   # PIDs of processes waiting to read
    semWritePID::Ptr{int32_t}  # PID of the process writing the data
    fifo_last_read::uint32_t   # Index of this reader's FIFO tail
    fifo_last_read_cnt0::uint64_t # cnt0 of this reader's FIFO tail
end


const libda = "libdao.so"

# ---------------------------------------------------------------------------
# Low-level wrappers over libdao's primary SHM API (see daoBase/include/dao.h
# for the full C signatures; the legacy daoShmImageCreate/daoShmShm2Img/...
# names still exist in libdao.so, but these wrappers call the new ones so
# this module stays consistent with the Python/MATLAB/Rust interfaces).
# ---------------------------------------------------------------------------

# daoShmCreate1D wrapper
function daoShmCreate1D(name::String, nbVal::UInt32)
    image_ptr = Ref{Ptr{Cvoid}}(C_NULL)
    result = ccall((:daoShmCreate1D, libda), Cint,
                   (Cstring, UInt32, Ref{Ptr{Cvoid}}), name, nbVal, image_ptr)
    return result, image_ptr[]
end

# daoShmOpen wrapper
function daoShmOpen(name::String, image::Ptr{IMAGE})
    result = ccall((:daoShmOpen, libda), Cint,
                   (Cstring, Ptr{IMAGE}), name, image)
    return result
end

# daoShmSetData wrapper
function daoShmSetData(image::Ptr{IMAGE}, im::Ptr{Cvoid}, nbVal::UInt32)
    result = ccall((:daoShmSetData, libda), Cint,
                   (Ptr{IMAGE}, Ptr{Cvoid}, UInt32), image, im, nbVal)
    return result
end

# daoShmSetDataPart wrapper
function daoShmSetDataPart(image::Ptr{IMAGE}, im::Ptr{UInt8}, nbVal::UInt32, position::UInt32, packetId::UInt16, packetTotal::UInt16, frameNumber::UInt64)
    result = ccall((:daoShmSetDataPart, libda), Cint,
                   (Ptr{IMAGE}, Ptr{UInt8}, UInt32, UInt32, UInt16, UInt16, UInt64), image, im, nbVal, position, packetId, packetTotal, frameNumber)
    return result
end

# daoShmSetDataPartFinalize wrapper
function daoShmSetDataPartFinalize(image::Ptr{IMAGE})
    result = ccall((:daoShmSetDataPartFinalize, libda), Cint,
                   (Ptr{IMAGE},), image)
    return result
end

# daoShmCreateSem wrapper
function daoShmCreateSem(image::Ptr{IMAGE}, NBsem::Int64)
    result = ccall((:daoShmCreateSem, libda), Cint,
                   (Ptr{IMAGE}, Int64), image, NBsem)
    return result
end

# daoShmCreate wrapper
function daoShmCreate(image::Ptr{IMAGE}, name::String, naxis::Int64, size::Ptr{UInt32}, atype::UInt8, shared::Cint, NBkw::Cint)
    result = ccall((:daoShmCreate, libda), Cint,
                   (Ptr{IMAGE}, Cstring, Int64, Ptr{UInt32}, UInt8, Cint, Cint), image, name, naxis, size, atype, shared, NBkw)
    return result
end

# daoShmCombine wrapper
function daoShmCombine(imageCube::Ptr{Ptr{IMAGE}}, image::Ptr{IMAGE}, nbChannel::Cint, nbVal::Cint)
    result = ccall((:daoShmCombine, libda), Cint,
                   (Ptr{Ptr{IMAGE}}, Ptr{IMAGE}, Cint, Cint), imageCube, image, nbChannel, nbVal)
    return result
end

# daoShmWaitSem wrapper
function daoShmWaitSem(image::Ptr{IMAGE}, semNb::Cint)
    result = ccall((:daoShmWaitSem, libda), Cint,
                   (Ptr{IMAGE}, Cint), image, semNb)
    return result
end

# daoShmWaitCounter wrapper
function daoShmWaitCounter(image::Ptr{IMAGE})
    result = ccall((:daoShmWaitCounter, libda), Cint,
                   (Ptr{IMAGE},), image)
    return result
end

# daoShmGetCounter wrapper
function daoShmGetCounter(image::Ptr{IMAGE})
    result = ccall((:daoShmGetCounter, libda), UInt64,
                   (Ptr{IMAGE},), image)
    return result
end

# daoShmClose wrapper
function daoShmClose(image::Ptr{IMAGE})
    result = ccall((:daoShmClose, libda), Cint,
                   (Ptr{IMAGE},), image)
    return result
end

# ---------------------------------------------------------------------------
# High-level, numpy/get_data-style interface, mirroring daoShm.py
# ---------------------------------------------------------------------------

function shm(name, data=nothing)
    if data == nothing
        println("connecting to existing $name")
        # if not parameters given, connect to existing SHM
        return connect_shm(name)
    else
        # else create or overwrite existing SHM
        println("$name will be created or overwritten")
        return create_shm(name, data)
    end
end

function create_shm(name, data)
    # create reference to an IMAGE
    image = Ref{dao.IMAGE}();
    # get the pointer
    image_ptr =  Base.unsafe_convert(Ptr{dao.IMAGE}, image);

    # get the size of the SHM
    naxis = length(size(data))
    println("naxis = $naxis")
    shmSize = size(data)
    shmSize = UInt32[UInt32(x) for x in shmSize]
    println("shmSize = $shmSize")

    # check type
    if eltype(data) == Int8
        atype = 1
    elseif eltype(data) == UInt8
        atype = 2
    elseif eltype(data) == Int16
        atype = 3
    elseif eltype(data) == UInt16
        atype = 4
    elseif eltype(data) == Int32
        atype = 5
    elseif eltype(data) == UInt32
        atype = 6
    elseif eltype(data) == Int64
        atype = 7
    elseif eltype(data) == UInt64
        atype = 8
    elseif eltype(data) == Float32
        atype = 9
    elseif eltype(data) == Float64
        atype = 10
    else
        error("Unsupported type")
    end
    println("atype = $atype")
    res = daoShmCreate(image_ptr, name, Int64(naxis), Ptr{UInt32}(pointer(shmSize)), UInt8(atype), Int32(1), Int32(0))
    println("SHM created")
    res = daoShmSetData(image_ptr, Ptr{Nothing}(pointer(data)), UInt32(length(data)))
    return image
end

function connect_shm(name)
    # Connect to an existing SHM
    # create reference to an IMAGE
    image = Ref{dao.IMAGE}();
    # get the pointer
    image_ptr =  Base.unsafe_convert(Ptr{dao.IMAGE}, image);
    # assume existing
    shm=dao.daoShmOpen(name, image_ptr);
    return image
end

function get_metadata(image)
    metadata=unsafe_load(image.x.md);
    return metadata
end

function get_counter(image)
    metadata = get_metadata(image);
    return Int(metadata.cnt0)
end

function get_naxis(image)
    metadata = get_metadata(image);
    return Int(metadata.naxis)
end

function get_size(image)
    # extract sizes
    metadata = get_metadata(image);
    size_x, size_y, size_z = metadata.size

    return Int(size_x), Int(size_y), Int(size_z)
end

function get_data(image, ;check::Bool=false, semNb::Int=0, spin::Bool=false)
    if check == true
        # get the pointer
        image_ptr =  Base.unsafe_convert(Ptr{dao.IMAGE}, image);
        if spin == true
            # wait for counter
            dao.daoShmWaitCounter(image_ptr)
        else
            # wait for semaphore
            dao.daoShmWaitSem(image_ptr, Int32(semNb))
        end
    end
    metadata = get_metadata(image);
    atype = Int(metadata.atype)
    # get axis size
    sx, sy, sz = dao.get_size(image)

    # based on type unwrap to the appropriate code
    if atype == 1
        data = unsafe_wrap(Array, Ptr{Int8}(image.x.array), (sx,sy))
    elseif atype == 2
        data = unsafe_wrap(Array, Ptr{UInt8}(image.x.array), (sx,sy))
    elseif atype == 3
        data = unsafe_wrap(Array, Ptr{Int16}(image.x.array), (sx,sy))
    elseif atype == 4
        data = unsafe_wrap(Array, Ptr{UInt16}(image.x.array), (sx,sy))
    elseif atype == 5
        data = unsafe_wrap(Array, Ptr{Int32}(image.x.array), (sx,sy))
    elseif atype == 6
        data = unsafe_wrap(Array, Ptr{UInt32}(image.x.array), (sx,sy))
    elseif atype == 7
        data = unsafe_wrap(Array, Ptr{Int64}(image.x.array), (sx,sy))
    elseif atype == 8
        data = unsafe_wrap(Array, Ptr{UInt64}(image.x.array), (sx,sy))
    elseif atype == 9
        data = unsafe_wrap(Array, Ptr{Float32}(image.x.array), (sx,sy))
    elseif atype == 10
        data = unsafe_wrap(Array, Ptr{Float64}(image.x.array), (sx,sy))
    else
        error("Unknown type")
    end
    return data
end

function set_data(image, data)
    metadata = get_metadata(image);
    atype = Int(metadata.atype)
    # get axis size
    sx, sy, sz = dao.get_size(image)

    # get pointer
    image_ptr =  Base.unsafe_convert(Ptr{dao.IMAGE}, image);
    dao.daoShmSetData(image_ptr, Ptr{Nothing}(pointer(data)), UInt32(sx * sy))
end

function close(image)
    # Release semaphores and unmap the SHM's metadata. Julia's GC will
    # eventually free the IMAGE struct itself once `image` is unreferenced;
    # this only releases the OS-level resources daoShmCreate/daoShmOpen
    # acquired.
    image_ptr = Base.unsafe_convert(Ptr{dao.IMAGE}, image);
    dao.daoShmClose(image_ptr)
end

end
