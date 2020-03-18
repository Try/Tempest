#pragma once

#include <Tempest/Point>

namespace Tempest {

class Transform {
  public:
    Transform()=default;
    Transform(float m11,float m12,float m13,
              float m21,float m22,float m23,
              float m31,float m32,float m33=1.f);

    void   map(float x,float y,float& outX,float& outY) const;
    PointF map(const Point& p) const { PointF r; map(float(p.x),float(p.y),r.x,r.y); return r; }

    void   translate(float x,float y);
    void   translate(const Point& p);

    static const Transform& identity();

  private:
    float v[3][3]={};
  };

inline void Transform::map(float x, float y, float &outX, float &outY) const {
  outX = v[0][0]*x + v[1][0]*y + v[2][0];
  outY = v[0][1]*x + v[1][1]*y + v[2][1];

  auto w = v[0][2]*x + v[1][2]*y + v[2][2];
  outX/=w;
  outY/=w;
  }

}
