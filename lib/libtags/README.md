libtags
=======

A cross-platform library for reading tags, designed for highly constrained environments.

Comparison to id3lib and taglib:

|                | libtags         | id3lib           | taglib           |
|:---------------|:----------------|:-----------------|:-----------------|
| ID3v2.4        | yes             | no               | yes              |
| Ogg/Vorbis     | yes             | no               | yes              |
| FLAC           | yes             | no               | yes              |
| m4a            | yes             | no               | yes              |
| opus           | yes             | no               | yes              |
| WAV            | yes             | no               | yes              |
| IT             | yes             | no               | ???              |
| XM             | yes             | no               | ???              |
| S3M            | yes             | no               | ???              |
| MOD            | yes             | no               | ???              |
| replay gain    | yes             | no               | ???              |
| size           | tiny            | bloated          | more bloated     |
| license        | MIT             | LGPL             | LGPL/MPL         |
| written in     | C               | C++              | C++              |
| memory         | no allocations  | allocates memory | allocates memory |
| thread safe    | yes             | ???              | ???              |
| speed          | ultra-fast      | slow             | fast             |
| tag writing    | no, not a goal  | yes              | yes              |
| Plan 9 support | yes, native     | no               | no               |

CPU time (784 files: mp3, ogg, flac):

|                | libtags          | taglib           |
|:---------------|:-----------------|:-----------------|
| files cached   | real    0m0.027s | real    0m0.155s |
|                | user    0m0.014s | user    0m0.102s |
|                | sys     0m0.012s | sys     0m0.053s |
|                |                  |                  |
| cache dropped  | real    0m1.158s | real    0m1.628s |
|                | user    0m0.024s | user    0m0.211s |
|                | sys     0m0.132s | sys     0m0.187s |

## Usage

Just compile it to an archive (`.a`) and link to your program. Use it in your code
by including `tags.h`, that's the API. Documentation is in the header.

See `examples/readtags.c`. You can compile it on Linux like so: `gcc examples/readtags.c *.c -I. -o readtags`.
