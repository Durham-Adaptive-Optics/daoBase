include(joinpath(ENV["DAOROOT"], "julia", "dao.jl"))

# connect to an existing SHM
image=dao.connect_shm("/tmp/test.im.shm");

# get nb axis
naxis = dao.get_naxis(image)

# get axis size
sx, sy, sz = dao.get_size(image)

# get counter (increase by 1 when soething is written in teh SHM)
cnt = dao.get_counter(image)

# get data
data=dao.get_data(image)

# get data, wait for counter to increase
data=dao.get_data(image, true, 0, true)