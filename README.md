# Dependencies Retriever

Original :
- URL : https://github.com/raphaelquati/ListDependency
- Author : Copyright Raphael Couto
- Original License : Apache License Version 2.0
- From commit : 8b8beff9aa761c4e62d5368414fb24fc980ab257

Modified/Altered :
- Author : Copyright 2024 Guillaume Guillet
- License : Apache License Version 2.0

## Description
This is a utility Windows app that retrieve all DLLs from a executable and list them.
You can also copy all DLLs to a directory and find DLLs recursively.

## Compile
You need MSYS2 environnement (**not tested on MSVC**) and CMake.

Just create a **build** directory at root and do :
```
cmake ..
cmake --build .
```

The executable should be ready to go and you can use it in a terminal like this :
```
depsRetriever --file YOUR_EXE --recursive --copy COPY_DLL_PATH --ignoreWindows
```

# Changelogs

## V1.0
This is the initial push after working on the original code.
- Remove all QT codes.
- Add [CLI11](https://github.com/CLIUtils/CLI11) library, adding command line options and arguments.
- Refactor of the original code using C++20.
