# cppgit

A git client built using C++ and libgit2

## Requirements

Ensure that you have the installed on your machine:

1. C++
2. CMake
3. libgit2

If it is necessarily, modify the [CMakeLists.txt](CMakeLists.txt) and set the value of GIT2_INCLUDE_DIR and 
GIT2_LIBRARY_DIR according to your installation.

## How to use it

Modify the following files:

1. [CppGit.cpp](CppGit.cpp), set the **username** and **password** variables with your data
2. [main.cpp](main.cpp), set the **url** and **local_path** with your data

Build and enjoy.
