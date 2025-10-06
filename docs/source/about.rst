What is DAO?
============

DAO (Duraham Adaptive Optics) is a high-performance, real-time software framework designed for adaptive optics systems. It provides a modular and flexible architecture that allows for easy integration of various components, such as wavefront sensors, deformable mirrors, and control algorithms. DAO is built to handle the demanding requirements of adaptive optics applications, including low latency, high throughput, and robust error handling.

It works using a series of shared memory blocks to pass data between processes. This allows for high-speed data transfer and low latency, which is essential for real-time applications. DAO also includes a state machine to manage the different states of the system, such as initialization, calibration, and operation but does not require this. At its heart DAO is a shared memory library with some extra features to make it easier to use in adaptive optics systems.


Repositories
------------
DAO is split into several repositories to make it easier to manage and maintain. The main repositories are:
- `daoBase <https://github.com/Durham-Adaptive-Optics/daoBase>`: The core library providing the shared memory functionality and basic data structures.
- `daoTools <https://github.com/Durham-Adaptive-Optics/daoTools>`: A collection of utility functions and tools for working with the DAO framework.
- `daoHw <https://github.com/Durham-Adaptive-Optics/daoHw>`: A hardware abstraction layer for interfacing with various hardware components used in adaptive optics systems.

daoBase
^^^^^^^
The daoBase repository contains the core library that provides the shared memory functionality and basic data structures. It is written in C++ and includes Python bindings for easy integration with Python applications. The library is designed to be lightweight and efficient, with a focus on low latency and high throughput.


daoTools
^^^^^^^^
The daoTools repository contains a collection of utility functions and tools for working with the DAO framework. It includes functions for data manipulation, file I/O, and other common tasks that are useful when developing adaptive optics applications. The daoTools library is designed to complement the daoBase library and provide additional functionality that is commonly needed in adaptive optics systems.

See the :doc:`daoTools` documentation for more details.


daoHw
^^^^^
The daoHw repository provides a hardware abstraction layer for interfacing with various hardware components used in adaptive optics systems. It includes drivers and interfaces for wavefront sensors, deformable mirrors, and other hardware devices commonly used in adaptive optics applications. The daoHw library is designed to be modular and extensible, allowing for easy integration of new hardware components as needed.

See the :doc:`daoHw` documentation for more details.