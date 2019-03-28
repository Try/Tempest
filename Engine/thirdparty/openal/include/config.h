#ifndef CONFIG_H
#define CONFIG_H

/* API declaration export attribute */

#define AL_API
#define AL_APIENTRY

/* Define to the library version */
#define ALSOFT_VERSION "${LIB_VERSION}"

/* Define any available alignment declaration */
#define ALIGN(x)
#ifdef __MINGW32__
#define align(x) aligned(x)
#endif

/* Define to the appropriate 'restrict' keyword */
#define RESTRICT

/* Define if we have the C11 aligned_alloc function */
// #define  HAVE_ALIGNED_ALLOC

/* Define if we have the posix_memalign function */
// #define  HAVE_POSIX_MEMALIGN

/* Define if we have the _aligned_malloc function */
// #define  HAVE__ALIGNED_MALLOC

/* Define if we have SSE CPU extensions */
//#define  HAVE_SSE 0

/* Define if we have ARM Neon CPU extensions */
//#define  HAVE_NEON 0

/* Define if we have the ALSA backend */
// #define  HAVE_ALSA

/* Define if we have the OSS backend */
// #define  HAVE_OSS

/* Define if we have the Solaris backend */
// #define  HAVE_SOLARIS

/* Define if we have the SndIO backend */
// #define  HAVE_SNDIO

/* Define if we have the MMDevApi backend */
// #define  HAVE_MMDEVAPI

/* Define if we have the DSound backend */
// #define  HAVE_DSOUND

/* Define if we have the Windows Multimedia backend */
// #define  HAVE_WINMM

/* Define if we have the PortAudio backend */
// #define  HAVE_PORTAUDIO

/* Define if we have the PulseAudio backend */
// #define  HAVE_PULSEAUDIO

/* Define if we have the CoreAudio backend */
// #define  HAVE_COREAUDIO

/* Define if we have the OpenSL backend */
// #define  HAVE_OPENSL

/* Define if we have the Wave Writer backend */
// #define  HAVE_WAVE

/* Define if we have the stat function */
// #define  HAVE_STAT

/* Define if we have the lrintf function */
// #define  HAVE_LRINTF

/* Define if we have the strtof function */
// #define  HAVE_STRTOF

/* Define if we have the __int64 type */
//#define  HAVE___INT64

/* Define to the size of a long int type */
// #define  SIZEOF_LONG ${SIZEOF_LONG}

/* Define to the size of a long long int type */
// #define  SIZEOF_LONG_LONG ${SIZEOF_LONG_LONG}

/* Define if we have GCC's destructor attribute */
#define  HAVE_GCC_DESTRUCTOR

/* Define if we have GCC's format attribute */
// #define  HAVE_GCC_FORMAT

/* Define if we have stdint.h */
#define HAVE_STDINT_H 1

/* Define if we have windows.h */
// #define  HAVE_WINDOWS_H

/* Define if we have dlfcn.h */
// #define  HAVE_DLFCN_H

/* Define if we have pthread_np.h */
// #define  HAVE_PTHREAD_NP_H

/* Define if we have xmmintrin.h */
// #define  HAVE_XMMINTRIN_H

/* Define if we have arm_neon.h */
// #define  HAVE_ARM_NEON_H

/* Define if we have malloc.h */
// #define  HAVE_MALLOC_H

/* Define if we have cpuid.h */
// #define  HAVE_CPUID_H

/* Define if we have guiddef.h */
// #define  HAVE_GUIDDEF_H

/* Define if we have initguid.h */
// #define  HAVE_INITGUID_H

/* Define if we have ieeefp.h */
// #define  HAVE_IEEEFP_H

/* Define if we have float.h */
// #define  HAVE_FLOAT_H

/* Define if we have fenv.h */
// #define  HAVE_FENV_H

/* Define if we have fesetround() */
// #define  HAVE_FESETROUND

/* Define if we have _controlfp() */
// #define  HAVE__CONTROLFP

/* Define if we have __control87_2() */
// #define  HAVE___CONTROL87_2

/* Define if we have pthread_setschedparam() */
// #define  HAVE_PTHREAD_SETSCHEDPARAM

#endif
