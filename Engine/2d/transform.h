#pragma once

#include <Tempest/Point>

namespace Tempest {

class Transform {
  public:
    Transform()=default;
    Transform(float m11,float m12,float m13,
              float m21,float m22,float m23,
              float m31,float m32,float m33=1.f);

    enum   Type : uint8_t {
      T_AxisAligned,
      T_None
      };

    void   map(float x,float y,float& outX,float& outY) const;
    void   map(int   x,int   y,int&   outX,int&   outY) const;
    PointF map(const Point& p) const { PointF r; map(float(p.x),float(p.y),r.x,r.y); return r; }

    void   translate(float x,float y);
    void   translate(const Point& p);

    void   rotate   (float angle);

    Type   type() const { return tp; }

    static const Transform& identity();

  private:
    float v[3][3]={};
    Type  tp = T_AxisAligned;

    void invalidateType();
  };

inline void Transform::map(float x, float y, float &outX, float &outY) const {
  outX = v[0][0]*x + v[1][0]*y + v[2][0];
  outY = v[0][1]*x + v[1][1]*y + v[2][1];

  auto w = v[0][2]*x + v[1][2]*y + v[2][2];
  outX/=w;
  outY/=w;
  }

inline void Transform::map(int x, int y, int &outX, int &outY) const {
  float ofx = v[0][0]*float(x) + v[1][0]*float(y) + v[2][0];
  float ofy = v[0][1]*float(x) + v[1][1]*float(y) + v[2][1];
  float w   = v[0][2]*float(x) + v[1][2]*float(y) + v[2][2];

  outX = int(ofx/w);
  outY = int(ofy/w);
  }

}
