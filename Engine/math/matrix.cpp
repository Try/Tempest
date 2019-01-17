#include "matrix.h"

#include <cmath>
#include <algorithm>

//#include <thirdparty/nv_math/nv_matrix.h>
// code is based on NvMath
#define KD_FLT_EPSILON 1.19209290E-07F
#define KD_DEG_TO_RAD_F 0.0174532924F

static void NvInvMat4x4f(float r[4][4], const float m[4][4])
{
    float d =
         m[0][0] * m[1][1] * m[2][2] * m[3][3] +
         m[0][0] * m[1][2] * m[2][3] * m[3][1] +
         m[0][0] * m[1][3] * m[2][1] * m[3][2] +
         m[0][1] * m[1][0] * m[2][3] * m[3][2] +
         m[0][1] * m[1][2] * m[2][0] * m[3][3] +
         m[0][1] * m[1][3] * m[2][2] * m[3][0] +
         m[0][2] * m[1][0] * m[2][1] * m[3][3] +
         m[0][2] * m[1][1] * m[2][3] * m[3][0] +
         m[0][2] * m[1][3] * m[2][0] * m[3][1] +
         m[0][3] * m[1][0] * m[2][2] * m[3][1] +
         m[0][3] * m[1][1] * m[2][0] * m[3][2] +
         m[0][3] * m[1][2] * m[2][1] * m[3][0] +
        -m[0][0] * m[1][1] * m[2][3] * m[3][2] +
        -m[0][0] * m[1][2] * m[2][1] * m[3][3] +
        -m[0][0] * m[1][3] * m[2][2] * m[3][1] +
        -m[0][1] * m[1][0] * m[2][2] * m[3][3] +
        -m[0][1] * m[1][2] * m[2][3] * m[3][0] +
        -m[0][1] * m[1][3] * m[2][0] * m[3][2] +
        -m[0][2] * m[1][0] * m[2][3] * m[3][1] +
        -m[0][2] * m[1][1] * m[2][0] * m[3][3] +
        -m[0][2] * m[1][3] * m[2][1] * m[3][0] +
        -m[0][3] * m[1][0] * m[2][1] * m[3][2] +
        -m[0][3] * m[1][1] * m[2][2] * m[3][0] +
        -m[0][3] * m[1][2] * m[2][0] * m[3][1];

    //assert(NvDifferentMatsf(r, m));

    r[0][0] = ( m[1][1] * m[2][2] * m[3][3] +
                m[1][2] * m[2][3] * m[3][1] +
                m[1][3] * m[2][1] * m[3][2] +
               -m[1][1] * m[2][3] * m[3][2] +
               -m[1][2] * m[2][1] * m[3][3] +
               -m[1][3] * m[2][2] * m[3][1]) / d;

    r[0][1] = ( m[0][1] * m[2][3] * m[3][2] +
                m[0][2] * m[2][1] * m[3][3] +
                m[0][3] * m[2][2] * m[3][1] +
               -m[0][1] * m[2][2] * m[3][3] +
               -m[0][2] * m[2][3] * m[3][1] +
               -m[0][3] * m[2][1] * m[3][2]) / d;

    r[0][2] = ( m[0][1] * m[1][2] * m[3][3] +
                m[0][2] * m[1][3] * m[3][1] +
                m[0][3] * m[1][1] * m[3][2] +
               -m[0][1] * m[1][3] * m[3][2] +
               -m[0][2] * m[1][1] * m[3][3] +
               -m[0][3] * m[1][2] * m[3][1]) / d;

    r[0][3] = ( m[0][1] * m[1][3] * m[2][2] +
                m[0][2] * m[1][1] * m[2][3] +
                m[0][3] * m[1][2] * m[2][1] +
               -m[0][1] * m[1][2] * m[2][3] +
               -m[0][2] * m[1][3] * m[2][1] +
               -m[0][3] * m[1][1] * m[2][2]) / d;

    r[1][0] = ( m[1][0] * m[2][3] * m[3][2] +
                m[1][2] * m[2][0] * m[3][3] +
                m[1][3] * m[2][2] * m[3][0] +
               -m[1][0] * m[2][2] * m[3][3] +
               -m[1][2] * m[2][3] * m[3][0] +
               -m[1][3] * m[2][0] * m[3][2] ) / d;

    r[1][1] = ( m[0][0] * m[2][2] * m[3][3] +
                m[0][2] * m[2][3] * m[3][0] +
                m[0][3] * m[2][0] * m[3][2] +
               -m[0][0] * m[2][3] * m[3][2] +
               -m[0][2] * m[2][0] * m[3][3] +
               -m[0][3] * m[2][2] * m[3][0]) / d;

    r[1][2] = ( m[0][0] * m[1][3] * m[3][2] +
                m[0][2] * m[1][0] * m[3][3] +
                m[0][3] * m[1][2] * m[3][0] +
               -m[0][0] * m[1][2] * m[3][3] +
               -m[0][2] * m[1][3] * m[3][0] +
               -m[0][3] * m[1][0] * m[3][2]) / d;

    r[1][3] = ( m[0][0] * m[1][2] * m[2][3] +
                m[0][2] * m[1][3] * m[2][0] +
                m[0][3] * m[1][0] * m[2][2] +
               -m[0][0] * m[1][3] * m[2][2] +
               -m[0][2] * m[1][0] * m[2][3] +
               -m[0][3] * m[1][2] * m[2][0]) / d;

    r[2][0] = ( m[1][0] * m[2][1] * m[3][3] +
                m[1][1] * m[2][3] * m[3][0] +
                m[1][3] * m[2][0] * m[3][1] +
               -m[1][0] * m[2][3] * m[3][1] +
               -m[1][1] * m[2][0] * m[3][3] +
               -m[1][3] * m[2][1] * m[3][0]) / d;

    r[2][1] = (-m[0][0] * m[2][1] * m[3][3] +
               -m[0][1] * m[2][3] * m[3][0] +
               -m[0][3] * m[2][0] * m[3][1] +
                m[0][0] * m[2][3] * m[3][1] +
                m[0][1] * m[2][0] * m[3][3] +
                m[0][3] * m[2][1] * m[3][0]) / d;

    r[2][2] = ( m[0][0] * m[1][1] * m[3][3] +
                m[0][1] * m[1][3] * m[3][0] +
                m[0][3] * m[1][0] * m[3][1] +
               -m[0][0] * m[1][3] * m[3][1] +
               -m[0][1] * m[1][0] * m[3][3] +
               -m[0][3] * m[1][1] * m[3][0]) / d;

    r[2][3] = ( m[0][0] * m[1][3] * m[2][1] +
                m[0][1] * m[1][0] * m[2][3] +
                m[0][3] * m[1][1] * m[2][0] +
               -m[0][1] * m[1][3] * m[2][0] +
               -m[0][3] * m[1][0] * m[2][1] +
               -m[0][0] * m[1][1] * m[2][3]) / d;

    r[3][0] = ( m[1][0] * m[2][2] * m[3][1] +
                m[1][1] * m[2][0] * m[3][2] +
                m[1][2] * m[2][1] * m[3][0] +
               -m[1][0] * m[2][1] * m[3][2] +
               -m[1][1] * m[2][2] * m[3][0] +
               -m[1][2] * m[2][0] * m[3][1]) / d;

    r[3][1] = ( m[0][0] * m[2][1] * m[3][2] +
                m[0][1] * m[2][2] * m[3][0] +
                m[0][2] * m[2][0] * m[3][1] +
               -m[0][0] * m[2][2] * m[3][1] +
               -m[0][1] * m[2][0] * m[3][2] +
               -m[0][2] * m[2][1] * m[3][0]) / d;

    r[3][2] = ( m[0][0] * m[1][2] * m[3][1] +
                m[0][1] * m[1][0] * m[3][2] +
                m[0][2] * m[1][1] * m[3][0] +
               -m[0][0] * m[1][1] * m[3][2] +
               -m[0][1] * m[1][2] * m[3][0] +
               -m[0][2] * m[1][0] * m[3][1]) / d;

    r[3][3] = ( m[0][0] * m[1][1] * m[2][2] +
                m[0][1] * m[1][2] * m[2][0] +
                m[0][2] * m[1][0] * m[2][1] +
               -m[0][0] * m[1][2] * m[2][1] +
               -m[0][1] * m[1][0] * m[2][2] +
               -m[0][2] * m[1][1] * m[2][0]) / d;
}

static void NvMultMat3x3f(float r[4][4], const float a[4][4], const float b[4][4])
{
    //assert(NvDifferentMatsf(r, a) && NvDifferentMatsf(r, b));

    r[0][0] = a[0][0]*b[0][0] + a[1][0]*b[0][1] + a[2][0]*b[0][2];
    r[0][1] = a[0][1]*b[0][0] + a[1][1]*b[0][1] + a[2][1]*b[0][2];
    r[0][2] = a[0][2]*b[0][0] + a[1][2]*b[0][1] + a[2][2]*b[0][2];
    r[0][3] = 0.0f;

    r[1][0] = a[0][0]*b[1][0] + a[1][0]*b[1][1] + a[2][0]*b[1][2];
    r[1][1] = a[0][1]*b[1][0] + a[1][1]*b[1][1] + a[2][1]*b[1][2];
    r[1][2] = a[0][2]*b[1][0] + a[1][2]*b[1][1] + a[2][2]*b[1][2];
    r[1][3] = 0.0f;

    r[2][0] = a[0][0]*b[2][0] + a[1][0]*b[2][1] + a[2][0]*b[2][2];
    r[2][1] = a[0][1]*b[2][0] + a[1][1]*b[2][1] + a[2][1]*b[2][2];
    r[2][2] = a[0][2]*b[2][0] + a[1][2]*b[2][1] + a[2][2]*b[2][2];
    r[2][3] = 0.0f;

    r[3][0] = 0.0f;
    r[3][1] = 0.0f;
    r[3][2] = 0.0f;
    r[3][3] = 1.0f;
}

static void NvMultMat4x3f(float r[4][4], const float a[4][4], const float b[4][4])
{
    //assert(NvDifferentMatsf(r, a) && NvDifferentMatsf(r, b));

    r[0][0] = a[0][0]*b[0][0] + a[1][0]*b[0][1] + a[2][0]*b[0][2];
    r[0][1] = a[0][1]*b[0][0] + a[1][1]*b[0][1] + a[2][1]*b[0][2];
    r[0][2] = a[0][2]*b[0][0] + a[1][2]*b[0][1] + a[2][2]*b[0][2];
    r[0][3] = 0.0f;

    r[1][0] = a[0][0]*b[1][0] + a[1][0]*b[1][1] + a[2][0]*b[1][2];
    r[1][1] = a[0][1]*b[1][0] + a[1][1]*b[1][1] + a[2][1]*b[1][2];
    r[1][2] = a[0][2]*b[1][0] + a[1][2]*b[1][1] + a[2][2]*b[1][2];
    r[1][3] = 0.0f;

    r[2][0] = a[0][0]*b[2][0] + a[1][0]*b[2][1] + a[2][0]*b[2][2];
    r[2][1] = a[0][1]*b[2][0] + a[1][1]*b[2][1] + a[2][1]*b[2][2];
    r[2][2] = a[0][2]*b[2][0] + a[1][2]*b[2][1] + a[2][2]*b[2][2];
    r[2][3] = 0.0f;

    r[3][0] = a[0][0]*b[3][0] + a[1][0]*b[3][1] + a[2][0]*b[3][2] + a[3][0];
    r[3][1] = a[0][1]*b[3][0] + a[1][1]*b[3][1] + a[2][1]*b[3][2] + a[3][1];
    r[3][2] = a[0][2]*b[3][0] + a[1][2]*b[3][1] + a[2][2]*b[3][2] + a[3][2];
    r[3][3] = 1.0f;
}

static void NvMultMat4x4f(float r[4][4], const float a[4][4], const float b[4][4])
{
    //assert(NvDifferentMatsf(r, a) && NvDifferentMatsf(r, b));

    r[0][0] = a[0][0]*b[0][0]+a[1][0]*b[0][1]+a[2][0]*b[0][2]+a[3][0]*b[0][3];
    r[0][1] = a[0][1]*b[0][0]+a[1][1]*b[0][1]+a[2][1]*b[0][2]+a[3][1]*b[0][3];
    r[0][2] = a[0][2]*b[0][0]+a[1][2]*b[0][1]+a[2][2]*b[0][2]+a[3][2]*b[0][3];
    r[0][3] = a[0][3]*b[0][0]+a[1][3]*b[0][1]+a[2][3]*b[0][2]+a[3][3]*b[0][3];

    r[1][0] = a[0][0]*b[1][0]+a[1][0]*b[1][1]+a[2][0]*b[1][2]+a[3][0]*b[1][3];
    r[1][1] = a[0][1]*b[1][0]+a[1][1]*b[1][1]+a[2][1]*b[1][2]+a[3][1]*b[1][3];
    r[1][2] = a[0][2]*b[1][0]+a[1][2]*b[1][1]+a[2][2]*b[1][2]+a[3][2]*b[1][3];
    r[1][3] = a[0][3]*b[1][0]+a[1][3]*b[1][1]+a[2][3]*b[1][2]+a[3][3]*b[1][3];

    r[2][0] = a[0][0]*b[2][0]+a[1][0]*b[2][1]+a[2][0]*b[2][2]+a[3][0]*b[2][3];
    r[2][1] = a[0][1]*b[2][0]+a[1][1]*b[2][1]+a[2][1]*b[2][2]+a[3][1]*b[2][3];
    r[2][2] = a[0][2]*b[2][0]+a[1][2]*b[2][1]+a[2][2]*b[2][2]+a[3][2]*b[2][3];
    r[2][3] = a[0][3]*b[2][0]+a[1][3]*b[2][1]+a[2][3]*b[2][2]+a[3][3]*b[2][3];

    r[3][0] = a[0][0]*b[3][0]+a[1][0]*b[3][1]+a[2][0]*b[3][2]+a[3][0]*b[3][3];
    r[3][1] = a[0][1]*b[3][0]+a[1][1]*b[3][1]+a[2][1]*b[3][2]+a[3][1]*b[3][3];
    r[3][2] = a[0][2]*b[3][0]+a[1][2]*b[3][1]+a[2][2]*b[3][2]+a[3][2]*b[3][3];
    r[3][3] = a[0][3]*b[3][0]+a[1][3]*b[3][1]+a[2][3]*b[3][2]+a[3][3]*b[3][3];
}

static void NvMultMatf(float result[4][4], const float a[4][4], const float b[4][4])
{
    /*
      Use a temporary matrix for the result and copy the temporary
      into the result when finished. Doing this instead of writing
      the result directly guarantees that the routine will work even
      if the result overlaps one or more of the arguments in
      memory.
    */

    float r[4][4];

    if ((a[0][3]==0.0f) &&
        (a[1][3]==0.0f) &&
        (a[2][3]==0.0f) &&
        (a[2][3]==1.0f) &&
        (b[0][3]==0.0f) &&
        (b[1][3]==0.0f) &&
        (b[2][3]==0.0f) &&
        (b[2][3]==1.0f))
    {
        if ((a[3][0]==0.0f) &&
            (a[3][1]==0.0f) &&
            (a[3][2]==0.0f) &&
            (b[3][0]==0.0f) &&
            (b[3][1]==0.0f) &&
            (b[3][2]==0.0f))
        {
            NvMultMat3x3f(r, a, b);
        }
        else
        {
            NvMultMat4x3f(r, a, b);
        }
    }
    else
    {
         NvMultMat4x4f(r, a, b);
    }

    std::memcpy(result, r, sizeof(r));
}


using namespace Tempest;

static_assert(std::is_trivially_copyable<Matrix4x4>::value,"must be trivial");

Matrix4x4::Matrix4x4( const float data[/*16*/] ){
    setData( data );
    }

Matrix4x4::Matrix4x4( float a11, float a12, float a13, float a14,
                      float a21, float a22, float a23, float a24,
                      float a31, float a32, float a33, float a34,
                      float a41, float a42, float a43, float a44 ){
  setData(a11, a12, a13, a14,
          a21, a22, a23, a24,
          a31, a32, a33, a34,
          a41, a42, a43, a44 );
  }

void Matrix4x4::identity(){
  setData( 1, 0, 0, 0,
           0, 1, 0, 0,
           0, 0, 1, 0,
           0, 0, 0, 1 );
  }

void Matrix4x4::rotate(float degrees, float x, float y, float z){
  float radians = degrees * KD_DEG_TO_RAD_F;
  float axis[3] = {x,y,z};
  float re[4][4];

  {
      /* build a quat first */

      float i, j, k, r;

      /* should be */
      float const dst_l = sinf(radians/2.0f);

      /* actually is */
      float const src_l = sqrtf(axis[0] * axis[0] +
                                      axis[1] * axis[1] +
                                      axis[2] * axis[2]);

      if (src_l <= KD_FLT_EPSILON) {
          i = 0.0f;
          j = 0.0f;
          k = 0.0f;
          r = 1.0f;
      } else {
          float const s = dst_l / src_l;
          i = axis[0] * s;
          j = axis[1] * s;
          k = axis[2] * s;
          r = cosf(radians/2.0f);
      }

      {
          /* build a matrix from the quat */

          float const a00 = 1.0f - 2.0f * (j * j + k * k);
          float const a01 = 2.0f * (i * j + r * k);
          float const a02 = 2.0f * (i * k - r * j);

          float const a10 = 2.0f * (i * j - r * k);
          float const a11 = 1.0f - 2.0f * (i * i + k * k);
          float const a12 = 2.0f * (j * k + r * i);

          float const a20 = 2.0f * (i * k + r * j);
          float const a21 = 2.0f * (j * k - r * i);
          float const a22 = 1.0f - 2.0f * (i * i + j * j);

          re[0][0] = m[0][0] * a00 + m[1][0] * a01 + m[2][0] * a02;
          re[0][1] = m[0][1] * a00 + m[1][1] * a01 + m[2][1] * a02;
          re[0][2] = m[0][2] * a00 + m[1][2] * a01 + m[2][2] * a02;
          re[0][3] = m[0][3] * a00 + m[1][3] * a01 + m[2][3] * a02;

          re[1][0] = m[0][0] * a10 + m[1][0] * a11 + m[2][0] * a12;
          re[1][1] = m[0][1] * a10 + m[1][1] * a11 + m[2][1] * a12;
          re[1][2] = m[0][2] * a10 + m[1][2] * a11 + m[2][2] * a12;
          re[1][3] = m[0][3] * a10 + m[1][3] * a11 + m[2][3] * a12;

          re[2][0] = m[0][0] * a20 + m[1][0] * a21 + m[2][0] * a22;
          re[2][1] = m[0][1] * a20 + m[1][1] * a21 + m[2][1] * a22;
          re[2][2] = m[0][2] * a20 + m[1][2] * a21 + m[2][2] * a22;
          re[2][3] = m[0][3] * a20 + m[1][3] * a21 + m[2][3] * a22;

          re[3][0] = m[3][0];
          re[3][1] = m[3][1];
          re[3][2] = m[3][2];
          re[3][3] = m[3][3];
      }
  }

  std::memcpy(m,re,sizeof(re));
  }

void Matrix4x4::rotateOX( float angle ){
  //NvMultRotXDegMatf(m,m,angle);
  angle *= KD_DEG_TO_RAD_F;
  const float s = std::sin(angle);
  const float c = std::cos(angle);

  float r[2][4];
  r[0][0] = m[1][0] *  c + m[2][0] * s;
  r[0][1] = m[1][1] *  c + m[2][1] * s;
  r[0][2] = m[1][2] *  c + m[2][2] * s;
  r[0][3] = m[1][3] *  c + m[2][3] * s;

  r[1][0] = m[1][0] * -s + m[2][0] * c;
  r[1][1] = m[1][1] * -s + m[2][1] * c;
  r[1][2] = m[1][2] * -s + m[2][2] * c;
  r[1][3] = m[1][3] * -s + m[2][3] * c;

  memcpy(m[1],r[0],4*sizeof(float));
  memcpy(m[2],r[1],4*sizeof(float));
  }

void Matrix4x4::rotateOY( float angle ){
  //NvMultRotYDegMatf(m,m,angle);
  angle *= KD_DEG_TO_RAD_F;
  const float s = sinf(angle);
  const float c = cosf(angle);

  float r[2][4];
  r[0][0] = m[0][0] * c + m[2][0] * -s;
  r[0][1] = m[0][1] * c + m[2][1] * -s;
  r[0][2] = m[0][2] * c + m[2][2] * -s;
  r[0][3] = m[0][3] * c + m[2][3] * -s;

  r[1][0] = m[0][0] * s + m[2][0] * c;
  r[1][1] = m[0][1] * s + m[2][1] * c;
  r[1][2] = m[0][2] * s + m[2][2] * c;
  r[1][3] = m[0][3] * s + m[2][3] * c;

  memcpy(m[0],r[0],4*sizeof(float));
  memcpy(m[2],r[1],4*sizeof(float));
  }

void Matrix4x4::rotateOZ( float angle ){
  //NvMultRotZDegMatf(m,m,angle);

  angle *= KD_DEG_TO_RAD_F;
  const float s = sinf(angle);
  const float c = cosf(angle);
  //NvMultRotZDegMatf(m,m,angle);
  //rotate(angle, 0, 0, 1);
  float r[2][4];
  r[0][0] = m[0][0] * c + m[1][0] * s;
  r[0][1] = m[0][1] * c + m[1][1] * s;
  r[0][2] = m[0][2] * c + m[1][2] * s;
  r[0][3] = m[0][3] * c + m[1][3] * s;

  r[1][0] = m[0][0] * -s + m[1][0] * c;
  r[1][1] = m[0][1] * -s + m[1][1] * c;
  r[1][2] = m[0][2] * -s + m[1][2] * c;
  r[1][3] = m[0][3] * -s + m[1][3] * c;

  memcpy(m[0],r[0],4*sizeof(float));
  memcpy(m[1],r[1],4*sizeof(float));
  }

const float* Matrix4x4::data() const{
  return &m[0][0];
  }

float Matrix4x4::at(int x, int y) const {
  return m[x][y];
  }

void Matrix4x4::setData( const float data[/*16*/]){
  memcpy(m, data, 16*sizeof(float) );
  }

void Matrix4x4::transpose(){
  for( int i=0; i<4; ++i )
    for( int r=0; r<i; ++r )
      std::swap( m[i][r], m[r][i] );
  }

void Matrix4x4::inverse(){
  for( int i=0; i<4; ++i )
    for( int r=0; r<i; ++r )
      std::swap( m[i][r], m[r][i] );
  float ret[4][4];
  NvInvMat4x4f( ret, m );
  std::memcpy(m,ret,sizeof(ret));
  //NvInvMatf( m, m );
  for( int i=0; i<4; ++i )
    for( int r=0; r<i; ++r )
      std::swap( m[i][r], m[r][i] );
  }

void Matrix4x4::mul( const Matrix4x4& other ){
  NvMultMatf( m, m, other.m );
  }

void Matrix4x4::setData( float a11, float a12, float a13, float a14,
                         float a21, float a22, float a23, float a24,
                         float a31, float a32, float a33, float a34,
                         float a41, float a42, float a43, float a44 ){
  m[0][0] = a11;
  m[1][0] = a12;
  m[2][0] = a13;
  m[3][0] = a14;

  m[0][1] = a21;
  m[1][1] = a22;
  m[2][1] = a23;
  m[3][1] = a24;

  m[0][2] = a31;
  m[1][2] = a32;
  m[2][2] = a33;
  m[3][2] = a34;

  m[0][3] = a41;
  m[1][3] = a42;
  m[2][3] = a43;
  m[3][3] = a44;
  }


void Matrix4x4::project( float   x, float   y, float   z, float   w,
                         float &ox, float &oy, float &oz, float &ow ) const {
  float a[4] = {x,y,z,w};

  ox = m[0][0] * a[0] + m[1][0] * a[1] + m[2][0] * a[2] + m[3][0] * a[3];
  oy = m[0][1] * a[0] + m[1][1] * a[1] + m[2][1] * a[2] + m[3][1] * a[3];
  oz = m[0][2] * a[0] + m[1][2] * a[1] + m[2][2] * a[2] + m[3][2] * a[3];
  ow = m[0][3] * a[0] + m[1][3] * a[1] + m[2][3] * a[2] + m[3][3] * a[3];
  }

void Matrix4x4::perspective(float angle, float aspect, float zNear, float zFar) {
  float range = float(tan( 3.141592654*(double(angle) / 2.0)/180.0 )) * zNear;
  float left = -range * aspect;
  float right = range * aspect;
  float bottom = -range;
  float top = range;

  for( int i=0; i<4; ++i )
    for( int r=0; r<4; ++r )
       m[i][r] = 0;

  m[0][0] = (float(2) * zNear) / (right - left);
  m[1][1] = (float(2) * zNear) / (top - bottom);
  m[2][2] = - (zFar + zNear) / (zFar - zNear);
  m[2][3] = - float(1);
  m[3][2] = - (float(2) * zFar * zNear) / (zFar - zNear);

  m[2][3] = 1;
  m[2][2] = zFar/(zFar-zNear);
  m[3][2] = -zNear*zFar/(zFar-zNear);
  }

void Tempest::Matrix4x4::set(int x, int y, float v) {
  m[x][y] = v;
  }

void Tempest::Matrix4x4::project(float &x, float &y, float &z, float &w) const {
  project( x,y,z,w,
           x,y,z,w );
  }

void Matrix4x4::project(float &x, float &y, float &z) const {
  float w = 1;
  project(x,y,z,w);
  x /= w;
  y /= w;
  z /= w;
  }
