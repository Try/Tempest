OUTFILE=minivorbis.h

OGGVER=1.3.4
OGGDIR=libogg-$OGGVER
OGGPKG=$OGGDIR.tar.gz
OGGURL=https://downloads.xiph.org/releases/ogg/$OGGPKG

VORBISVER=1.3.7
VORBISDIR=libvorbis-$VORBISVER
VORBISPKG=$VORBISDIR.tar.gz
VORBISURL=https://downloads.xiph.org/releases/vorbis/$VORBISPKG

rm -f $OUTFILE

if [ ! -d "$OGGDIR" ]; then
    rm -rf $OGGPKG
    wget $OGGURL
    tar xzf $OGGPKG
fi

if [ ! -d "$VORBISDIR" ]; then
    rm -rf $VORBISPKG
    wget $VORBISURL
    tar xzf $VORBISPKG
fi

cat <<EOF >> $OUTFILE
/*
  minivorbis.h -- libvorbis decoder in a single header
  Project URL: https://github.com/edubart/minivorbis

  This is libogg 1.3.4 + libvorbis 1.3.7 contained in a single header
  to be bundled in C/C++ applications with ease for decoding OGG sound files.
  Ogg Vorbis is a open general-purpose compressed audio format
  for mid to high quality audio and music at fixed and variable bitrates.

  Do the following in *one* C file to implement Ogg and Vorbis:
    #define OGG_IMPL
    #define VORBIS_IMPL

  Optionally provide the following defines:
    OV_EXCLUDE_STATIC_CALLBACKS     - exclude the default static callbacks

  Note that almost no modification was made in the Ogg/Vorbis implementation code,
  thus there are some C variable names that may collide with your code,
  therefore it is best to declare the implementation in dedicated C file.

  LICENSE
    BSD-like License, same as libogg and libvorbis, see end of file.
*/
EOF

# ogg headers
cat $OGGDIR/include/ogg/os_types.h >> $OUTFILE
cat $OGGDIR/include/ogg/ogg.h >> $OUTFILE
sed -i "s@# *include <ogg/config_types.h>@/* config_types.h */\n\
#include <stdint.h>\n\
typedef int16_t ogg_int16_t;\n\
typedef uint16_t ogg_uint16_t;\n\
typedef int32_t ogg_int32_t;\n\
typedef uint32_t ogg_uint32_t;\n\
typedef int64_t ogg_int64_t;\n\
typedef uint64_t ogg_uint64_t;@" $OUTFILE

# vorbis headers
cat $VORBISDIR/include/vorbis/codec.h >> $OUTFILE
cat $VORBISDIR/include/vorbis/vorbisfile.h >> $OUTFILE

# ogg implementation
echo "#ifdef OGG_IMPL" >> $OUTFILE
cat <<EOF >> $OUTFILE
#ifdef __cplusplus
extern "C" {
#endif
EOF
  cat $OGGDIR/src/bitwise.c >> $OUTFILE
  cat $OGGDIR/src/framing.c >> $OUTFILE
  sed -i "/#include \"crctable.h\"/r $OGGDIR/src/crctable.h" $OUTFILE
cat <<EOF >> $OUTFILE
#ifdef __cplusplus
}
#endif
EOF
echo "#endif /* OGG_IMPL */" >> $OUTFILE

# vorbis implementation
echo "#ifdef VORBIS_IMPL" >> $OUTFILE
cat <<EOF >> $OUTFILE
#ifdef __cplusplus
extern "C" {
#endif
EOF
  # vorbis internal headers
  cat $VORBISDIR/lib/misc.h >> $OUTFILE
  cat $VORBISDIR/lib/os.h >> $OUTFILE
  cat $VORBISDIR/lib/mdct.h >> $OUTFILE
  cat $VORBISDIR/lib/envelope.h >> $OUTFILE
  cat $VORBISDIR/lib/codebook.h >> $OUTFILE
  cat $VORBISDIR/lib/smallft.h >> $OUTFILE
  cat $VORBISDIR/lib/codec_internal.h >> $OUTFILE
  sed -i "/#include \"psy.h\"/r $VORBISDIR/lib/psy.h" $OUTFILE
  sed -i "/#include \"backends.h\"/r $VORBISDIR/lib/backends.h" $OUTFILE
  sed -i "/#include \"bitrate.h\"/r $VORBISDIR/lib/bitrate.h" $OUTFILE
  sed -i "/#include \"highlevel.h\"/r $VORBISDIR/lib/highlevel.h" $OUTFILE
  cat $VORBISDIR/lib/lookup_data.h >> $OUTFILE
  cat $VORBISDIR/lib/lookup.h >> $OUTFILE
  cat $VORBISDIR/lib/lpc.h >> $OUTFILE
  cat $VORBISDIR/lib/lsp.h >> $OUTFILE
  cat $VORBISDIR/lib/masking.h >> $OUTFILE
  cat $VORBISDIR/lib/registry.h >> $OUTFILE
  cat $VORBISDIR/lib/scales.h >> $OUTFILE
  cat $VORBISDIR/lib/window.h >> $OUTFILE

  # vorbis sources
  cat $VORBISDIR/lib/mdct.c >> $OUTFILE
  cat $VORBISDIR/lib/smallft.c >> $OUTFILE
  cat $VORBISDIR/lib/block.c >> $OUTFILE
  cat $VORBISDIR/lib/envelope.c >> $OUTFILE
  cat $VORBISDIR/lib/window.c >> $OUTFILE
  cat $VORBISDIR/lib/lsp.c >> $OUTFILE
  cat $VORBISDIR/lib/lpc.c >> $OUTFILE
  cat $VORBISDIR/lib/analysis.c >> $OUTFILE
  cat $VORBISDIR/lib/synthesis.c >> $OUTFILE
  cat $VORBISDIR/lib/psy.c >> $OUTFILE
  cat $VORBISDIR/lib/info.c >> $OUTFILE
  sed -i 's/FLOOR1_fromdB_LOOKUP/_FLOOR1_fromdB_LOOKUP/' $OUTFILE
  cat $VORBISDIR/lib/floor1.c >> $OUTFILE
  cat $VORBISDIR/lib/floor0.c >> $OUTFILE
  cat $VORBISDIR/lib/res0.c >> $OUTFILE
  cat $VORBISDIR/lib/mapping0.c >> $OUTFILE
  cat $VORBISDIR/lib/registry.c >> $OUTFILE
  cat $VORBISDIR/lib/codebook.c >> $OUTFILE
  sed -i 's/bitreverse/_bitreverse/' $OUTFILE
  cat $VORBISDIR/lib/sharedbook.c >> $OUTFILE
  cat $VORBISDIR/lib/lookup.c >> $OUTFILE
  cat $VORBISDIR/lib/bitrate.c >> $OUTFILE

  # vorbis file
  cat $VORBISDIR/lib/vorbisfile.c >> $OUTFILE
cat <<EOF >> $OUTFILE
#ifdef __cplusplus
}
#endif
EOF
echo "#endif /* VORBIS_IMPL */" >> $OUTFILE

# ogg/vorbis license
cat <<EOF >> $OUTFILE
/*
Copyright (c) 2002-2020 Xiph.org Foundation
Copyright (c) 2020 Eduardo Bart (https://github.com/edubart)

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

- Redistributions of source code must retain the above copyright
notice, this list of conditions and the following disclaimer.

- Redistributions in binary form must reproduce the above copyright
notice, this list of conditions and the following disclaimer in the
documentation and/or other materials provided with the distribution.

- Neither the name of the Xiph.org Foundation nor the names of its
contributors may be used to endorse or promote products derived from
this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION
OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
EOF

# Comment all include headers
sed -i 's/#include "\([^"]*\)"/\/\*#include "\1"\*\//' $OUTFILE
sed -i 's/# *include <ogg\/\([^>]*\)>/\/\*#include <ogg\/\1>\*\//' $OUTFILE
