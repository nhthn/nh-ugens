**NHHall** is an open source algorithmic reverb unit in a single C++11 header file (`src/core/nh_hall.hpp`). Features:

- Allpass loop topology with delay line modulaton for a lush 90's IDM sound
- True stereo signal path with controllable spread
- Infinite hold support
- Respectable CPU use
- Sample-rate independence
- No dependencies outside the C++ standard library
- Bring your own real-time safe memory allocator (no unwanted `malloc` calls!)
- Permissive MIT license
- Clean, readable source code for easy modification

It was written specifically for [SuperCollider], [ChucK], and [Auraglyph] in response to the lack of good reverbs on these platforms.

[SuperCollider]: https://supercollider.github.io/
[ChucK]: http://chuck.stanford.edu/
[Auraglyph]: http://auraglyph.com/

## Compilation

Building for SuperCollider is available through an [sc3-plugins PR](https://github.com/supercollider/sc3-plugins/pull/193).

To compile, first create a build directory:

    mkdir build
    cd build

To build for ChucK, turn on the `CHUCK` flag and set `CHUCK_PATH` correctly:

    cmake -DCHUCK=ON ..
    cmake -DCHUCK_PATH=/path/to/chuck ..

Here, `/path/to/chuck` is the path to a copy of the ChucK source code.

All CMake options set with `-D` are persistent, so there is no need to run them multiple times unless you are changing values.

After picking your options, run `make`.
