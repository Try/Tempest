#pragma once

#include <cstring>

namespace Tempest{

class Matrix4x4 final {
  public:
    Matrix4x4()=default;
    Matrix4x4( const Matrix4x4& other );
    Matrix4x4( const float data[/*16*/] );
    Matrix4x4( float a11, float a12, float a13, float a14,
               float a21, float a22, float a23, float a24,
               float a31, float a32, float a33, float a34,
               float a41, float a42, float a43, float a44 );
    ~Matrix4x4();

    void identity();

    void translate( const float v[/*3*/]){
      translate( v[0], v[1], v[2] );
      }

    inline void translate(float x, float y, float z){
      m[3][0] = m[0][0] * x + m[1][0] * y + m[2][0] * z + m[3][0];
      m[3][1] = m[0][1] * x + m[1][1] * y + m[2][1] * z + m[3][1];
      m[3][2] = m[0][2] * x + m[1][2] * y + m[2][2] * z + m[3][2];
      m[3][3] = m[0][3] * x + m[1][3] * y + m[2][3] * z + m[3][3];
      }


    inline void scale( float x ){
      scale(x,x,x);
      }

    inline void scale(float x, float y, float z){
      m[0][0] *= x;
      m[0][1] *= x;
      m[0][2] *= x;
      m[0][3] *= x;

      m[1][0] *= y;
      m[1][1] *= y;
      m[1][2] *= y;
      m[1][3] *= y;

      m[2][0] *= z;
      m[2][1] *= z;
      m[2][2] *= z;
      m[2][3] *= z;
      }

    void rotate(float angle, float x, float y, float z);
    void rotateOX( float angle );
    void rotateOY( float angle );
    void rotateOZ( float angle );

    const float *data() const;
    float at( int x, int y ) const;
    void  set( int x, int y, float v );

    void setData( const float data[/*16*/]);
    void setData( float a11, float a12, float a13, float a14,
                  float a21, float a22, float a23, float a24,
                  float a31, float a32, float a33, float a34,
                  float a41, float a42, float a43, float a44 );

    void transpose();
    void inverse();
    void mul( const Matrix4x4& other );

    void project(float   x, float   y, float   z, float   w,
                 float &ox, float &oy, float &oz, float &ow ) const;
    void project(float & x, float & y, float & z, float & w ) const;
    void project(float & x, float & y, float & z ) const;

    void perspective( float angle, float aspect, float zNear, float zFar);

    Matrix4x4& operator = ( const Matrix4x4& other ) {
      std::memcpy(this,&other,sizeof(other));
      return *this;
      }

    bool operator == ( const Matrix4x4& other ) const {
      return std::memcmp(this,&other,sizeof(*this))==0;
      }
    bool operator != ( const Matrix4x4& other ) const{
      return std::memcmp(this,&other,sizeof(*this))!=0;
      }
  private:
    float m[4][4]={};
  };

}
