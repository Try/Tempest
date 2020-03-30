#include "transform.h"

#include <cmath>
#include <cstring>

#define KD_FLT_EPSILON 1.19209290E-07F
#define KD_DEG_TO_RAD_F 0.0174532924F

using namespace Tempest;

Transform::Transform(float m11, float m12, float m13,
                     float m21, float m22, float m23,
                     float m31, float m32, float m33) {
  v[0][0]=m11;
  v[0][1]=m12;
  v[0][2]=m13;

  v[1][0]=m21;
  v[1][1]=m22;
  v[1][2]=m23;

  v[2][0]=m31;
  v[2][1]=m32;
  v[2][2]=m33;
  invalidateType();
  }

void Transform::translate(float x, float y) {
  v[2][0] = v[0][0] * x + v[1][0] * y + v[2][0];
  v[2][1] = v[0][1] * x + v[1][1] * y + v[2][1];
  v[2][2] = v[0][2] * x + v[1][2] * y + v[2][2];
  invalidateType();
  }

void Transform::translate(const Point& p) {
  translate(float(p.x),float(p.y));
  }

void Transform::rotate(float angle) {
  double dangle = double(angle)*(3.14159265359/180.0);
  const float s = float(std::sin(dangle));
  const float c = float(std::cos(dangle));

  float r[2][2];
  r[0][0] = v[0][0] * c + v[1][0] * s;
  r[0][1] = v[0][1] * c + v[1][1] * s;

  r[1][0] = v[0][0] * -s + v[1][0] * c;
  r[1][1] = v[0][1] * -s + v[1][1] * c;

  for(int i=0;i<2;++i)
    for(int j=0;j<2;++j)
      if(std::fabs(r[i][j])<KD_FLT_EPSILON)
        r[i][j]=0.f;

  memcpy(v[0],r[0],2*sizeof(float));
  memcpy(v[1],r[1],2*sizeof(float));

  invalidateType();
  }

const Transform &Transform::identity() {
  static Transform tr(1,0,0,
                      0,1,0,
                      0,0,1);
  return tr;
  }

void Transform::invalidateType() {
  if((v[0][1]==0.f && v[0][2]==0.f && v[1][0]==0.f && v[1][2]==0.f) ||
     (v[0][0]==0.f && v[0][2]==0.f && v[1][1]==0.f && v[1][2]==0.f))
    tp = T_AxisAligned; else
    tp = T_None;
  }
