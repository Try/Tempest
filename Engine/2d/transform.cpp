#include "transform.h"

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
  }

void Transform::translate(float x, float y) {
  v[2][0] = v[0][0] * x + v[1][0] * y + v[2][0];
  v[2][1] = v[0][1] * x + v[1][1] * y + v[2][1];
  v[2][2] = v[0][2] * x + v[1][2] * y + v[2][2];
  }

void Transform::translate(const Point& p) {
  translate(p.x,p.y);
  }

const Transform &Transform::identity() {
  static Transform tr(1,0,0,
                      0,1,0,
                      0,0,1);
  return tr;
  }
