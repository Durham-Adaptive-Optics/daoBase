Quick Start Guide
=================

This is the Quick Start guide for your project. In this page we will the discuss the use of share memory mainly via the python interface for examples. The theory and stratergies can be used in C/C++ as well.


Create a shared memory file
---------------------------
To create a shared memory file, you can use the following code:

.. code-block:: python

    import dao 
    import numpy as np


    shm = dao.shm('/tmp/file.im.shm', np.zeros([10,10]).astype(np.uint16))

The typical location for shared memory files is /tmp/ but you can use any location that is writable by the user. a character limit of 255 characters applies to the full path.

From python is created using a numpy array and supports multi dimensional arrays. 

Note: semaphores are created automatically when the shared memory file is created, these will be created in /dev/shm/ and will have the same name as the shared memory file with a .sem extension. so if two files in two seperate locations are created with the same handle the semaphore will be shared.

Reading
-------

To open an ecisting shared memory file and read the data use the following code:

.. code-block:: python

    import dao 

    shm = dao.shm('file.im.shm')
    data = shm.get_data()

Here a numpy data array is not passed as the data type and shape are read from the shared memory file. We use get_data() to read the data without checking any semaphores.

Writing
-------

.. code-block:: python

    import dao 

    shm = dao.shm('file.im.shm')
    data = np.zeros([10,10]).astype(np.uint16)
    data = shm.set_data(data) # write data to shared memory 

Syncronisation
--------------
.. code-block:: python

    data = shm.get_data(check=True)

This waits for the next write to the SHM, then reads the data. This is useful when reading data that is being written to by another process.

If multiple processes are reading from the same shared memory file then they can all read at the same time: each SHM has several semaphores (16 by default, so up to 16 readers), the writer posts all of them, and each reader waits on its own. dao picks it for you -- one no other reader waits on -- at the first wait, and keeps it until the SHM is closed or the process ends (even if it crashes). No semaphore number to choose, and two readers never share one (they would each miss frames).

.. note::
    The first ``get_data(check=True)`` waits for the next write: writes made before the reader came do not count.

A semaphore number can still be given (``get_data(check=True, semNb=5)``); dao warns if another reader already waits on it.

The other way to use the semaphore is to spin on a counter in the SHM file to change. This forces the code to stay in usersapace and block until the shm is updated. This is very useful in real-time processes as the function returns quicker then using the semaphore but it adds complexity to the system. This is really only useful for C/C++ tasks that need to run at high speed.

.. code-block:: python

    data = shm.get_data(spin=True)

Simple Loop
~~~~~~~~~~~

Writer example

.. code-block:: python

    import dao
    import numpy as np

    data = np.zeros([10,10]).astype(np.uint16)
    shm = dao.shm('/tmp/file.im.shm', data)
    while True:
        data += 1
        data = shm.set_data(data)
        time.sleep(0.1)

Reader example

.. code-block:: python
    
    import dao

    shm = dao.shm('/tmp/file.im.shm')
    while True:
        data = shm.get_data(check=True)
        print(data)


