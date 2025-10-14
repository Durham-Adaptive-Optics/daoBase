.. DAO documentation master file

Durham Adaptive Optics (DAO)
============================

.. raw:: html

   <div class="hero-section">
      <h1>Durham Adaptive Optics</h1>
      <p class="tagline">High-Performance Real-Time Framework for Adaptive Optics Systems</p>
      <p class="description">
         DAO is a modular, high-performance software framework designed for demanding adaptive optics applications. 
         Built around shared memory architecture, it delivers low-latency data transfer, robust error handling, 
         and cross-platform compatibility for scientific computing and real-time control systems.
      </p>
      <div class="cta-buttons">
         <a href="quick_start_guide.html" class="cta-button">Get Started</a>
         <a href="installation_guide.html" class="cta-button secondary">Install DAO</a>
      </div>
   </div>

Why Choose DAO?
===============

.. raw:: html

   <div class="feature-grid">
      <div class="feature-card">
         <h3><span class="icon">⚡</span>Ultra-Low Latency</h3>
         <p>Shared memory architecture enables microsecond-level data transfer between processes, essential for real-time adaptive optics control loops.</p>
      </div>
      <div class="feature-card">
         <h3><span class="icon">🔧</span>Modular Design</h3>
         <p>Flexible component-based architecture allows easy integration of wavefront sensors, deformable mirrors, and custom control algorithms.</p>
      </div>
      <div class="feature-card">
         <h3><span class="icon">🌐</span>Cross-Platform</h3>
         <p>Full support for Linux, macOS, and Windows with consistent APIs and performance across all platforms.</p>
      </div>
      <div class="feature-card">
         <h3><span class="icon">🔬</span>Scientific Computing</h3>
         <p>Built-in support for multi-dimensional arrays, various data types, and seamless integration with Python and C++ workflows.</p>
      </div>
      <div class="feature-card">
         <h3><span class="icon">🛡️</span>Robust & Reliable</h3>
         <p>Advanced state machine management, comprehensive error handling, and real-time considerations for mission-critical applications.</p>
      </div>
      <div class="feature-card">
         <h3><span class="icon">📊</span>High Throughput</h3>
         <p>Optimized for high-speed data processing with NUMA awareness, thread management, and efficient memory handling.</p>
      </div>
   </div>

Quick Start
===========

.. raw:: html

   <div class="quick-start">
      <h2>Get Up and Running in Minutes</h2>
      <p>Create your first shared memory block and start passing data between processes:</p>

.. code-block:: python

   import dao 
   import numpy as np

   # Create a shared memory block
   shm = dao.shm('/tmp/mydata.im.shm', np.zeros([1024, 1024], dtype=np.uint16))
   
   # Write data from one process
   shm.set_data(my_camera_image)
   
   # Read from another process
   data = shm.get_data()

.. raw:: html

   </div>

Repository Structure
====================

.. raw:: html

   <div class="repo-badges">
      <a href="https://github.com/Durham-Adaptive-Optics/daoBase" class="repo-badge">🏗️ daoBase - Core Library</a>
      <a href="https://github.com/Durham-Adaptive-Optics/daoTools" class="repo-badge">🛠️ daoTools - Utilities</a>
      <a href="https://github.com/Durham-Adaptive-Optics/daoHw" class="repo-badge">⚙️ daoHw - Hardware Layer</a>
   </div>

DAO is organized into three main repositories, each serving a specific purpose in the adaptive optics ecosystem:

**daoBase** - The foundational shared memory library providing ultra-fast inter-process communication, data structures, and synchronization mechanisms.

**daoTools** - Essential utilities and helper functions for data manipulation, file I/O, and common adaptive optics tasks.

**daoHw** - Hardware abstraction layer supporting various wavefront sensors, deformable mirrors, and other AO hardware components.

Documentation Guide
===================

.. raw:: html

   <div class="section-group">
      <h2>🚀 Getting Started</h2>
      <div class="doc-grid">
         <a href="installation_guide.html" class="doc-link">
            <h4>Installation Guide</h4>
            <p>Complete setup instructions for all supported platforms</p>
         </a>
         <a href="quick_start_guide.html" class="doc-link">
            <h4>Quick Start Guide</h4>
            <p>Your first DAO application in minutes</p>
         </a>
         <a href="about.html" class="doc-link">
            <h4>About DAO</h4>
            <p>Project overview and architecture principles</p>
         </a>
      </div>
   </div>

   <div class="section-group">
      <h2>💡 Core Concepts</h2>
      <div class="doc-grid">
         <a href="shared_memory.html" class="doc-link">
            <h4>Shared Memory System</h4>
            <p>Low-latency inter-process communication foundation</p>
         </a>
         <a href="threads.html" class="doc-link">
            <h4>Thread Management</h4>
            <p>Real-time thread system and lifecycle management</p>
         </a>
         <a href="state_machine.html" class="doc-link">
            <h4>State Machine</h4>
            <p>System state management and transitions</p>
         </a>
         <a href="daoComponent.html" class="doc-link">
            <h4>DAO Components</h4>
            <p>Modular component architecture and interfaces</p>
         </a>
      </div>
   </div>

   <div class="section-group">
      <h2>🔧 Advanced Topics</h2>
      <div class="doc-grid">
         <a href="numa.html" class="doc-link">
            <h4>NUMA Optimization</h4>
            <p>Memory architecture considerations for performance</p>
         </a>
         <a href="doubleBuffer.html" class="doc-link">
            <h4>Double Buffering</h4>
            <p>Advanced buffering strategies for smooth data flow</p>
         </a>
         <a href="tuning_guide.html" class="doc-link">
            <h4>Performance Tuning</h4>
            <p>Optimization techniques for maximum performance</p>
         </a>
         <a href="logging.html" class="doc-link">
            <h4>Logging & Debugging</h4>
            <p>Comprehensive logging and diagnostic tools</p>
         </a>
      </div>
   </div>

   <div class="section-group">
      <h2>📚 Reference & Support</h2>
      <div class="doc-grid">
         <a href="api_reference.html" class="doc-link">
            <h4>API Reference</h4>
            <p>Complete function and class documentation</p>
         </a>
         <a href="daoTools.html" class="doc-link">
            <h4>DAO Tools</h4>
            <p>Utility functions and helper libraries</p>
         </a>
         <a href="daoHw.html" class="doc-link">
            <h4>Hardware Support</h4>
            <p>Hardware abstraction and device drivers</p>
         </a>
         <a href="troubleshooting.html" class="doc-link">
            <h4>Troubleshooting</h4>
            <p>Common issues and solutions</p>
         </a>
      </div>
   </div>


daolite - Durham Adaptive Optics Latency Investigation and Timing Estimator 
===========================================================================

Need help optimising your adaptive optics real-time control system? We've recently released daolite, a tool for modelling and reducing computational latency in AO systems. daolite lets you build complex pipelines from a library of hardware components, estimate timing for each stage, compare configurations across CPUs, GPUs, and network architectures, and rapidly iterate on designs to meet your real-time performance requirements.

.. raw:: html

   <div class="hero-section">
      <img src="_static/daoliteLogoCrop.png" alt="daolite" class="hero-logo"/>
      <div class="hero-text">
        <p class="tagline">Model and optimise computational latency in Adaptive Optics real-time control systems.</p>
        <p class="description">Estimate per-component timing, compare hardware configurations (CPUs, GPUs, network), and iterate on pipeline designs to meet real-time constraints.</p>
        <div class="cta-buttons">
           <a href="https://daobase.readthedocs.io/en/latest/index.html" class="cta-button">Documentation</a>
           <a href="https://github.com/davetbarr/daolite" class="cta-button secondary">Github</a>
        </div>
      </div>
   </div>


.. toctree::
   :maxdepth: 2
   :hidden:
   :caption: Getting Started

   about
   installation_guide
   quick_start_guide
   cite

.. toctree::
   :maxdepth: 2
   :hidden:
   :caption: Core Concepts

   shared_memory
   daoComponent
   threads
   state_machine
   doubleBuffer
   daoroot_daodata

.. toctree::
   :maxdepth: 2
   :hidden:
   :caption: Advanced Topics

   numa
   tuning_guide
   logging
   troubleshooting

.. toctree::
   :maxdepth: 2
   :hidden:
   :caption: Reference

   api_reference
   daoTools
   daoHw

Indices and Tables
==================

* :ref:`genindex`
* :ref:`modindex`
* :ref:`search`
