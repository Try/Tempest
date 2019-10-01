#pragma once

#include <cstdint>

namespace Tempest {
  namespace Detail {
    typedef uint32_t PTR32;
    typedef uint32_t DWORD;
    typedef uint16_t WORD;

    struct DDCOLORKEY {
      DWORD       dwColorSpaceLowValue;
      DWORD       dwColorSpaceHighValue;
      };

#pragma pack(push,1)
    struct DDPIXELFORMAT {
      DWORD       dwSize;                 // size of structure
      DWORD       dwFlags;                // pixel format flags
      DWORD       dwFourCC;               // (FOURCC code)
      union {
        DWORD   dwRGBBitCount;          // how many bits per pixel
        DWORD   dwYUVBitCount;          // how many bits per pixel
        DWORD   dwZBufferBitDepth;      // how many total bits/pixel in z buffer (including any stencil bits)
        DWORD   dwAlphaBitDepth;        // how many bits for alpha channels
        DWORD   dwLuminanceBitCount;    // how many bits per pixel
        DWORD   dwBumpBitCount;         // how many bits per "buxel", total
        DWORD   dwPrivateFormatBitCount;// Bits per pixel of private driver formats. Only valid in texture
        // format list and if DDPF_D3DFORMAT is set
        };
      union {
        DWORD   dwRBitMask;             // mask for red bit
        DWORD   dwYBitMask;             // mask for Y bits
        DWORD   dwStencilBitDepth;      // how many stencil bits (note: dwZBufferBitDepth-dwStencilBitDepth is total Z-only bits)
        DWORD   dwLuminanceBitMask;     // mask for luminance bits
        DWORD   dwBumpDuBitMask;        // mask for bump map U delta bits
        DWORD   dwOperations;           // DDPF_D3DFORMAT Operations
        };
      union
      {
        DWORD   dwGBitMask;             // mask for green bits
        DWORD   dwUBitMask;             // mask for U bits
        DWORD   dwZBitMask;             // mask for Z bits
        DWORD   dwBumpDvBitMask;        // mask for bump map V delta bits
        struct {
          WORD    wFlipMSTypes;       // Multisample methods supported via flip for this D3DFORMAT
          WORD    wBltMSTypes;        // Multisample methods supported via blt for this D3DFORMAT
          } MultiSampleCaps;
        };
      union
      {
        DWORD   dwBBitMask;             // mask for blue bits
        DWORD   dwVBitMask;             // mask for V bits
        DWORD   dwStencilBitMask;       // mask for stencil bits
        DWORD   dwBumpLuminanceBitMask; // mask for luminance in bump map
        };

      union {
        DWORD   dwRGBAlphaBitMask;      // mask for alpha channel
        DWORD   dwYUVAlphaBitMask;      // mask for alpha channel
        DWORD   dwLuminanceAlphaBitMask;// mask for alpha channel
        DWORD   dwRGBZBitMask;          // mask for Z channel
        DWORD   dwYUVZBitMask;          // mask for Z channel
        };
      };

    struct DDSCAPS2 {
      DWORD       dwCaps;         // capabilities of surface wanted
      DWORD       dwCaps2;
      DWORD       dwCaps3;
      union {
        DWORD       dwCaps4;
        DWORD       dwVolumeDepth;
        };
      };

    struct DDSURFACEDESC2 {
      DWORD      dwSize;
      DWORD      dwFlags;
      DWORD      dwHeight;
      DWORD      dwWidth;
      union {
        int32_t  lPitch;
        DWORD dwLinearSize;
        };
      union {
        DWORD dwBackBufferCount;
        DWORD dwDepth;
        };
      union {
        DWORD dwMipMapCount;
        DWORD dwRefreshRate;
        DWORD dwSrcVBHandle;
        };
      DWORD      dwAlphaBitDepth;
      DWORD      dwReserved;
      PTR32      lpSurface;
      union {
        DDCOLORKEY ddckCKDestOverlay;
        DWORD      dwEmptyFaceColor;
        };
      DDCOLORKEY ddckCKDestBlt;
      DDCOLORKEY ddckCKSrcOverlay;
      DDCOLORKEY ddckCKSrcBlt;
      union {
        DDPIXELFORMAT ddpfPixelFormat;
        DWORD         dwFVF;
        };
      DDSCAPS2   ddsCaps;
      DWORD      dwTextureStage;
      };

    const unsigned int FOURCC_DXT1 = 827611204;
    const unsigned int FOURCC_DXT3 = 861165636;
    const unsigned int FOURCC_DXT5 = 894720068;
    }
#pragma pack(pop)
  }
