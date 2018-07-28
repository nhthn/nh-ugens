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

It was written specifically for [SuperCollider] and [ChucK].

[SuperCollider]: https://supercollider.github.io/
[ChucK]: http://chuck.stanford.edu/

## Install

- SuperCollider users: NHHall is in the [sc3-plugins](https://github.com/supercollider/sc3-plugins) distribution. As of June 2018, you will need to build the latest unreleased version of sc3-plugins to get this UGen.
- ChucK users: NHHall is in the [chugins](https://github.com/ccrma/chugins) distribution. As of June 2018, you will need to build the latest unreleased version of sc3-plugins to get this UGen.
