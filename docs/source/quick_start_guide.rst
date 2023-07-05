Quick Start Guide
==================

This is the Quick Start guide for your project. In this page we will the discuss the use of share memory mainly via the python interface for examples. The theory and stratergies can be used in C/C++ as well.


Reading
-------
.. code-block:: python

    import dao 

    shared_memory = dao.shm('file.im.shm')
    data = shared_memory.get_data()

Writing
-------

.. code-block:: python

    import dao 

    shared_memory = dao.shm('file.im.shm')
    data = shared_memory.set_data(np.zeros([10,10]).astype(np.uint16))

Syncronisation
--------------
.. code-block:: python

    data = shared_memory.get_data(check=True)



Simple Loop
~~~~~~~~~~~
.. code-block:: python

    while True:
        data = shared_memory.get_data(check=True)
        print()


