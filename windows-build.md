# daoBase for Windows

These build instructions are only temporary measures until we can get the Waf script working.

## Building the DLL

Open the "x64 Native Tools Command Prompt" included with Microsoft Visual Studio, and navigate to the src/c folder. Run the following command
```
cl /LD /W3 /D_CRT_SECURE_NO_WARNINGS /I ..\..\include dao.c
```

This should create a dao.dll file in the src/c folder.

## Using with the Python bindings

The Python bindings require the compiled protobuf .py files. For testing, I have taken these from the build folder of a working Linux DAO installation.

Alter line 31 of daoShm.py to point to the dao.dll file on your system.
