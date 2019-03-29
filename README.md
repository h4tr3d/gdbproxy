asio-gdbproxy
=============

R&D gdbproxy implementation to allow hooks some requests and do some useful staff.

Base idea, allow to debug threads on RTOSes with hardware debuggers that does not
provide support for them.

Basically for MicroBlaze core with FreeRTOS using with Xilinx SDK.

In such scheme, `asio-gdbproxy` should be used between `hw_server` and `mb-gdb`.

To allow it, `mb-gdb` must be configured to use any non-standard port, i.e. 4002.

Source code requires Asio >= 1.12 & CMake >= 3.10.  Use following commands to configure &
build example:

    mkdir build
    cd build
    cmake ..
    make


