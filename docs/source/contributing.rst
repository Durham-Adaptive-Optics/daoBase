Contributing to daoBase
=======================

Thank you for contributing to daoBase. This page documents the coding standards,
linting toolchain, and workflow expected for all contributions.

.. contents:: On this page
   :local:
   :depth: 2

----

Setting Up for Development
--------------------------

After cloning the repository, install the pre-commit hooks so that linting runs
automatically on every commit::

   pip install pre-commit
   pre-commit install

You can also run all linters manually across the full repository at any time::

   pre-commit run --all-files

The first run will download the hook environments; subsequent runs are fast.

.. note::

   The Rust linter (``clippy``) requires the C library to be compiled first
   (``waf build``).  If ``build/include/dao.h`` is not present, the clippy hook
   is skipped automatically and a notice is printed.

----

Coding Standards
----------------

The repository contains four primary languages. Each has a dedicated formatter
and linter configured at the repository root.  The rules below summarise the
enforced conventions; the config files are the authoritative source of truth.

C and C++
~~~~~~~~~

**Formatter**: ``clang-format`` (config: :file:`.clang-format`)

**Linter**: ``clang-tidy`` (config: :file:`.clang-tidy`)

.. list-table::
   :widths: 30 70
   :header-rows: 1

   * - Rule
     - Detail
   * - Brace style
     - **Allman** — opening brace always on its own line
   * - Indentation
     - 4 spaces, no tabs
   * - Line length
     - 140 characters maximum
   * - Pointer/reference
     - Left-aligned (``T* ptr``, ``T& ref``)
   * - Namespace indentation
     - Contents indented inside ``namespace Dao { }``
   * - Access modifiers
     - ``public:`` / ``private:`` / ``protected:`` at the same level as class body (``AccessModifierOffset: 0``)
   * - Naming — namespace
     - ``PascalCase``  (``Dao``)
   * - Naming — class / struct
     - ``PascalCase``  (``DaoShm``, ``ShmSync``)
   * - Naming — public methods
     - ``PascalCase``  (``GetData()``)
   * - Naming — private methods
     - ``camelCase``  (``parseHeader()``)
   * - Naming — member variables
     - ``m_`` prefix + ``camelCase``  (``m_frameCount``)
   * - Naming — parameters / locals
     - ``camelCase``
   * - Naming — constants / macros
     - ``UPPER_SNAKE_CASE``
   * - Header guards
     - ``#ifndef DAO_FOO_HPP`` / ``#define DAO_FOO_HPP``
   * - Include order
     - Project headers ``"dao*.h"`` first, then system ``<...>`` headers, then third-party ``<...>`` headers
   * - Documentation
     - Doxygen ``/** ... */`` blocks on all public API declarations

**Example**:

.. code-block:: cpp

   /**
    * @brief Brief description of the class.
    */
   class MyComponent
   {
   public:
       /**
        * @brief Create a component.
        * @param name Unique identifier string.
        */
       explicit MyComponent(const std::string& name);

       void Start();
       void Stop();

   private:
       void processFrame();

       std::string m_name;
       uint32_t    m_frameCount { 0 };
   };

Python
~~~~~~

**Formatter + linter**: ``ruff`` (config: :file:`pyproject.toml`)

.. list-table::
   :widths: 30 70
   :header-rows: 1

   * - Rule
     - Detail
   * - Style base
     - PEP 8
   * - Indentation
     - 4 spaces, no tabs
   * - Line length
     - 140 characters maximum
   * - Quote style
     - Double quotes ``"``
   * - Import order
     - stdlib → third-party → local (isort via ruff ``I`` rules)
   * - Naming — functions / variables
     - ``snake_case``
   * - Naming — classes
     - ``daoFoo`` convention is permitted for legacy API classes; new standalone classes should use ``PascalCase``
   * - Docstrings
     - Google-style (preferred) or NumPy-style

Enabled ruff rule sets: ``E``, ``W`` (pycodestyle), ``F`` (pyflakes),
``I`` (isort), ``N`` (pep8-naming, with ``N801`` relaxed), ``UP`` (pyupgrade),
``B`` (bugbear), ``C4`` (comprehensions), ``RUF`` (ruff-specific).

**Example**:

.. code-block:: python

   import ctypes
   import logging
   from threading import Thread

   import numpy as np

   import daoLog


   class daoShm:
       """Read/write access to a DAO shared memory segment."""

       def __init__(self, name: str, data: np.ndarray) -> None:
           self.name = name
           self._data = data

       def get_data(self) -> np.ndarray:
           """Return the current frame."""
           return self._data

Rust
~~~~

**Formatter**: ``rustfmt`` (config: :file:`rustfmt.toml`)

**Linter**: ``clippy`` (warnings promoted to errors: ``-D warnings``)

.. list-table::
   :widths: 30 70
   :header-rows: 1

   * - Rule
     - Detail
   * - Brace style
     - ``AlwaysNextLine`` (matches Allman used in C/C++)
   * - Indentation
     - 4 spaces
   * - Line length
     - 140 characters
   * - Import grouping
     - ``StdExternalCrate`` — std, then external, then crate-local
   * - Trailing commas
     - Vertical (one item per line when wrapping)
   * - Edition
     - 2021

Julia
~~~~~

**Formatter**: ``JuliaFormatter.jl`` (config: :file:`.JuliaFormatter.toml`)

.. list-table::
   :widths: 30 70
   :header-rows: 1

   * - Rule
     - Detail
   * - Indentation
     - 4 spaces
   * - Line length
     - 140 characters
   * - Line endings
     - Unix (``\n``)

----

Running Linters Individually
-----------------------------

If you want to run a single linter rather than the full pre-commit suite:

.. code-block:: bash

   # C/C++ — reformat in-place
   clang-format -i --style=file include/daoShm.hpp

   # C/C++ — static analysis (requires a compile_commands.json)
   clang-tidy include/daoShm.hpp -- -I include -std=c++17

   # Python — lint with auto-fix
   ruff check --fix src/python/

   # Python — reformat
   ruff format src/python/

   # Rust — reformat
   cargo fmt --manifest-path src/rust/Cargo.toml

   # Rust — lint
   cargo clippy --manifest-path src/rust/Cargo.toml -- -D warnings

   # Julia — reformat
   julia -e 'using JuliaFormatter; format(".")'

----

Contribution Workflow
---------------------

1. **Branch** from ``main`` using a descriptive name, e.g. ``feature/fifo-python-api`` or ``fix/shm-timeout``.
2. **Write code** following the standards above.
3. **Commit** — the pre-commit hooks will run automatically.  If a hook reformats a file, stage the changes and commit again.
4. **Test** — run the existing test suite before opening a pull request.
5. **Pull request** — target ``main``; include a brief description of the change and any relevant issue numbers.

.. note::

   If your branch predates the introduction of the linter config files, run
   ``pre-commit run --all-files`` on your branch and commit the reformatted
   files as a single dedicated commit (e.g. ``style: apply clang-format and ruff``)
   before making functional changes.  This keeps the functional diff reviewable.

----

New Linter Config Files
-----------------------

The following configuration files were added when the linting toolchain was
introduced.  They must be present at the repository root for the hooks to work:

.. list-table::
   :widths: 30 70
   :header-rows: 1

   * - File
     - Purpose
   * - :file:`.clang-format`
     - clang-format style rules for C and C++
   * - :file:`.clang-tidy`
     - clang-tidy static analysis checks and naming conventions
   * - :file:`pyproject.toml`
     - ruff formatter and linter configuration for Python
   * - :file:`rustfmt.toml`
     - rustfmt style rules for Rust
   * - :file:`.JuliaFormatter.toml`
     - JuliaFormatter style rules for Julia
   * - :file:`.pre-commit-config.yaml`
     - pre-commit hook definitions tying all of the above together
   * - :file:`src/rust/.cargo/config.toml`
     - Cargo build flags (``-L`` path to ``libdao.so``); required for Rust builds
