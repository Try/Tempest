# MiniVorbis

This is libogg 1.3.4 + libvorbis 1.3.7 contained in a single header
to be bundled in C/C++ applications with ease for decoding OGG sound files.
[Ogg Vorbis](https://en.wikipedia.org/wiki/Vorbis) is a open general-purpose compressed audio format
for mid to high quality audio and music at fixed and variable bitrates.

## Example Usage

```c
#define OGG_IMPL
#define VORBIS_IMPL
#include "minivorbis.h"

int main(int argc, char** argv)
{
    if(argc < 2) {
        printf("No input file.\n");
        return -1;
    }
    /* Open sound file. */
    FILE* fp = fopen(argv[1], "rb");
    if(!fp) {
        printf("Failed to open file '%s'.", argv[1]);
        return -1;
    }
    /* Open sound stream. */
    OggVorbis_File vorbis;
    if(ov_open_callbacks(fp, &vorbis, NULL, 0, OV_CALLBACKS_DEFAULT) != 0) {
        printf("Invalid Ogg file '%s'.", argv[1]);
        return -1;
    }
    /* Print sound information. */
    vorbis_info* info = ov_info(&vorbis, -1);
    printf("Ogg file %d Hz, %d channels, %d kbit/s.\n", info->rate, info->channels, info->bitrate_nominal / 1024);
    /* Read the entire sound stream. */
    unsigned char buf[4096];
    while(1) {
        int section = 0;
        long bytes = ov_read(&vorbis, buf, sizeof(buf), 0, 2, 1, &section);
        if(bytes <= 0) /* end of file or error */
            break;
    }
    /* Close sound file */
    ov_clear(&vorbis);
    return 0;
}
```

## Usage

Copy `minivorbis.h` into your C/C++ project, include it anywhere you want to use the Vorbis API.
Then do the following in *one* C file to implement Ogg and Vorbis:
```c
#define OGG_IMPL
#define VORBIS_IMPL
#include "minivorbis.h"
```

Optionally provide the following defines:
  - `OV_EXCLUDE_STATIC_CALLBACKS`     - exclude the default static callbacks

Note that almost no modification was made in the Ogg/Vorbis implementation code,
thus there are some C variable names that may collide with your code,
therefore it is best to declare the implementation in dedicated C file.

## Documentation

For documentation on how to use Ogg and Vorbis read its [official documentation](https://xiph.org/doc/).

## Extra Miniaudio Vorbis Decoder

You can use `miniaudio_vorbis.h` to replace Vorbis decoder of the awesome
single-header [miniaudio](https://miniaud.io/) library with minivorbis, follow the
instructions described on it.

## Updates

- **02-Nov-2020**: Library created, using Ogg 1.3.4 and Vorbis 1.3.7.

## Notes

The header is generated using the bash script `gen.sh` all modifications done is there.

## License

BSD-like License, same as libogg and libvorbis, see LICENSE.txt for information.
