include(joinpath(ENV["DAOROOT"], "julia", "dao.jl"))

# connect to an existing SHM
# create new ones
shm1=dao.shm("/tmp/test1.im.shm", ones(Float64, 10, 10));
# example for UINt16 array
shm2=dao.shm("/tmp/test2.im.shm", ones(UInt16, 10, 10));
# connect to an existing one for example the one we created shm1, can be created in python, C, etc...
shm=dao.shm("/tmp/test1.im.shm");

# get nb axis
naxis = dao.get_naxis(shm)

# get axis size
sx, sy, sz = dao.get_size(shm)

# get counter (increase by 1 when soething is written in teh SHM)
cnt = dao.get_counter(shm)

# get data
data=dao.get_data(shm)

# get data, wait for semaphore 0 to be released
data=dao.get_data(shm, check=true)
# get data, wait for semaphore 1 to be released
data=dao.get_data(shm, check=true, semNb=1)
# get data, wait for counter to increase
data=dao.get_data(shm, check=true, spin=true)

# set data in the SHM (just adding 1 and write in the SHM)
dao.set_data(shm, data.+1);
