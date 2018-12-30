## NHHall

**NHHall** is an open source algorithmic reverb unit in a single C++11 header file (`src/core/nh_hall.hpp`). Features:

- Allpass loop topology with random modulation for a lush 90's IDM sound
- True stereo signal path with controllable spread
- Infinite hold support
- Respectable CPU use
- Permissive MIT license
- No dependencies outside the C++ standard library
- Bring your own real-time safe memory allocator (no unwanted `malloc` calls!)
- Sample rate independence, with SR specified at run time, not compile tiime
- Clean, readable source code for easy modification

### Install

- SuperCollider users: NHHall is in the [sc3-plugins](https://github.com/supercollider/sc3-plugins) distribution. You will need version 3.10 or above. See the NHHall help file for usage.
- ChucK users: NHHall is in the [chugins](https://github.com/ccrma/chugins) distribution. As of December 2018, you will need to build the latest unreleased version of chugins to get this UGen. See [this example code](https://github.com/ccrma/chugins/blob/master/NHHall/nhhall-help.ck) for usage.
