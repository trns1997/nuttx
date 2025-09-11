.. include:: /substitutions.rst
.. _cpp_cmake:

=======================
C++ Example using CMake
=======================

In some situations, developers intend to implement software using the NuttX platform in
a previously set hardware and configuration where it is not possible or allowed to make
changes. In such situations, less contact with the operating source tree is better, where
it is only used for the application.

Some approaches are possible to do that today:

* https://cwiki.apache.org/confluence/display/NUTTX/Building+NuttX+with+Applications+Outside+of+the+Source+Tree
* https://www.programmersought.com/article/61604062421/

We have been seen the increase of the use of C++ language in embedded systems application. And
CMake (https://www.cmake.org) is the preferred build system used to build C++ projects. NuttX
support C++ based projects.

Using the 'build as a library' procedure of NuttX, it is possible to build NuttX
applications using C++ language and also the cmake build tool.

This document will show how to reimplement the hellocpp project using this cmake.

Preparation
===========

#. Base NuttX compilation changes

    For this example, load the configuration 'stm32f4discovery:nsh' for building

    .. code-block:: console

       $ cd nuttx
       $ ./tools/configure.sh -E -l stm32f4discovery:nsh

    In menuconfig, the main points to be changed on a typical NuttX configuration are the following:

    * RTOS Features -> Tasks and Scheduling -> Application entry point to 'hellocpp_main'
    * Board Selection -> Board common logic -> to enable board common start up logic
    * Library Routines -> Have C++ compiler
    * Library Routines -> Have C++ initialization -> C++ Library -> Toolchain C++ support (you can also choose the basic version or the LLVM one)
    * Library Routines -> Have C++ initialization -> C++ Library -> C++ low level library select -> GNU low level libsupc++
    * Library Routines -> Language standard -> choose the version you want - for this example we will use "c++17"
    * Library Routines -> Enable Exception Support -> to enable to support C++ exceptions - for this example we will select it
    * Library Routines -> Enable RTTI Support -> to enable to support C++ RTTI features (like dynamic_cast()/typeid()) - for this example we will not enable it


    Build NuttX and generate the export

    .. code-block:: console

       $ make export

Creating the project
====================

#. Create your project file structure

    The project structure is organized as follow:

    .. code-block:: console

       hellocpp/
       hellocpp/CMakeLists.txt
       hellocpp/nuttx-export-12.10.0/
       hellocpp/include/HelloWorld.hpp
       hellocpp/src/main.cpp
       hellocpp/src/HelloWorld.cpp

    The directory 'nuttx-export-12.10.0' is the unzipped content from the file created during
    make export procedure done before.

#. File contents

* hellocpp/CMakeLists.txt

.. code-block:: cmake

    cmake_minimum_required(VERSION 3.12...3.31)

    project(HelloCpp
        VERSION 1.0
        DESCRIPTION "Hello world C++ NuttX"
    )

    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)

    # Use CMAKE_SOURCE_DIR for source files
    set(SOURCE_FILES
        ${CMAKE_SOURCE_DIR}/src/HelloWorld.cpp
        ${CMAKE_SOURCE_DIR}/src/main.cpp
    )

    set(EXE_NAME hello)

    add_executable(${EXE_NAME} ${SOURCE_FILES})

    # Add include directory so you can #include "..." from include/
    target_include_directories(${EXE_NAME}
            PRIVATE
            ${CMAKE_SOURCE_DIR}/include
    )

    # Generate a .bin file from the ELF after build
    add_custom_command(
        TARGET ${EXE_NAME}
        POST_BUILD
        COMMAND ${CMAKE_OBJCOPY} -S -O binary
                ${CMAKE_BINARY_DIR}/${EXE_NAME}
                ${CMAKE_BINARY_DIR}/${EXE_NAME}.bin
        COMMENT "Generating binary image ${EXE_NAME}.bin"
    )


* hellocpp/include/HelloWorld.hpp

.. code-block:: c++

    #pragma once

    class CHelloWorld
    {
    public:
        CHelloWorld();
        ~CHelloWorld() = default;

        bool HelloWorld();

    private:
        int mSecret;
    };


* hellocpp/src/main.cpp

.. code-block:: c++

    #include <memory>

    #include "HelloWorld.hpp"

    int hellocpp_main(int, char*[])
    {
        auto pHelloWorld = std::make_shared<CHelloWorld>();
        pHelloWorld->HelloWorld();

        CHelloWorld helloWorld;
        helloWorld.HelloWorld();

        return 0;
    }


* hellocpp/src/HelloWorld.cpp

.. code-block:: c++

    #include <cstdio>
    #include <string>

    #include "HelloWorld.hpp"

    CHelloWorld::CHelloWorld()
    {
        mSecret = 42;
        std::printf("Constructor: mSecret=%d\n",mSecret);
    }


    bool CHelloWorld::HelloWorld()
    {
        std::printf("HelloWorld: mSecret=%d\n",mSecret);

        std::string sentence = "Hello";
        std::printf("TEST=%s\n",sentence.c_str());

        if (mSecret == 42)
        {
                std::printf("CHelloWorld: HelloWorld: Hello, world!\n");
                return true;
        }
        else
        {
                std::printf("CHelloWorld: HelloWorld: CONSTRUCTION FAILED!\n");
                return false;
        }
    }


Building
========

To launch build, you use the cmake procedure:

.. code-block:: console

    $ mkdir build
    $ cd build
    $ cmake .. -DCMAKE_TOOLCHAIN_FILE=../nuttx-export-12.10.0/scripts/toolchain.cmake
    $ make

You should find an ELF file named hello in build/src directory. You can now flash it with the appropriate tool (you may need to convert it to bin format with a tool like objcopy)
