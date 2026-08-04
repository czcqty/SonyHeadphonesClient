# Third-party dependencies

`CMakeLists.txt` declares the project's fetched third-party dependencies.
Currently this is `fmt`, pinned to an explicit release and linked through its
CMake target.

Qt is an external build prerequisite and is discovered with `find_package`; it
is not downloaded by this directory.
