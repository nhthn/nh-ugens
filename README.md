## nh-ugens

To compile, first create a build directory:

    mkdir build
    cd build

To build for SuperCollider, turn on the `SUPERCOLLIDER` flag and set `SC_PATH` correctly:

    cmake -DSUPERCOLLIDER=ON ..
    cmake -DSC_PATH=/path/to/supercollider ..

Here, `/path/to/supercollider` is the path to a copy of the SuperCollider **source code**.

To build for ChucK, turn on the `CHUCK` flag and set `CHUCK_PATH` correctly:

    cmake -DCHUCK=ON ..
    cmake -DCHUCK_PATH=/path/to/chuck ..

Here, `/path/to/chuck` is the path to a copy of the ChucK source code.

All CMake options set with `-D` are persistent, so there is no need to run them multiple times unless you are changing values.

After picking your options, run `make`.
