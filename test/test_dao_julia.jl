using DAO
# if you want to stick with namespacing all the function names, you can use
# `import DAO` instead

# connect to an existing SHM
# create new ones
shm1 = shm("/tmp/test1.im.shm", ones(Float64, 10, 10));
# example for UINt16 array
shm2 = shm("/tmp/test2.im.shm", ones(UInt16, 10, 10));
# connect to an existing one for example the one we created shm1, can be created in python, C, etc...
shm3 = shm("/tmp/test1.im.shm");

# get nb axis
naxis = get_naxis(shm3)

# get axis size
sx, sy, sz = get_size(shm3)

# get counter (increase by 1 when soething is written in teh SHM)
cnt = get_counter(shm3)

# get data
data = get_data(shm3)

# get data, wait for semaphore 0 to be released
data = get_data(shm3, check=true)
# get data, wait for semaphore 1 to be released
data = get_data(shm3, check=true, semNb=1)
# get data, wait for counter to increase
data = get_data(shm3, check=true, spin=true)

# set data in the SHM (just adding 1 and write in the SHM)
set_data(shm3, data.+1);
