#pragma once

#include <cstdint>
#include <cmath>
#include <algorithm>

namespace Tempest {

template<class T,size_t len>
class BasicPoint;

template<class T>
struct BasicSize;

template<class T>
struct BasicRect;

enum Orientation:uint8_t {
  Horizontal=0,
  Vertical  =1
  };


template<class T>
class BasicPoint<T,1> {
  public:
    BasicPoint()=default;
    BasicPoint(T x):x(x){}

    BasicPoint& operator -= ( const BasicPoint & p ){ x-=p.x; return *this; }
    BasicPoint& operator += ( const BasicPoint & p ){ x+=p.x; return *this; }

    BasicPoint& operator /= ( const T& p ){ x/=p; return *this; }
    BasicPoint& operator *= ( const T& p ){ x*=p; return *this; }

    friend BasicPoint operator - ( BasicPoint l,const BasicPoint & r ){ l-=r; return l; }
    friend BasicPoint operator + ( BasicPoint l,const BasicPoint & r ){ l+=r; return l; }

    friend BasicPoint operator / ( BasicPoint l,const T r ){ l/=r; return l; }
    friend BasicPoint operator * ( BasicPoint l,const T r ){ l*=r; return l; }

    BasicPoint operator - () const { return BasicPoint(-x); }

    T manhattanLength() const { return x; }
    T quadLength()      const { return x; }

    bool operator ==( const BasicPoint & other ) const { return x==other.x; }
    bool operator !=( const BasicPoint & other ) const { return x!=other.x; }

    T x=T();
  };

template<class T>
class BasicPoint<T,2> {
  public:
    BasicPoint()=default;
    BasicPoint(T x,T y):x(x),y(y){}

    BasicPoint& operator -= ( const BasicPoint & p ){ x-=p.x; y-=p.y; return *this; }
    BasicPoint& operator += ( const BasicPoint & p ){ x+=p.x; y+=p.y; return *this; }

    BasicPoint& operator /= ( const T& p ){ x/=p; y/=p; return *this; }
    BasicPoint& operator *= ( const T& p ){ x*=p; y*=p; return *this; }

    friend BasicPoint operator - ( BasicPoint l,const BasicPoint & r ){ l-=r; return l; }
    friend BasicPoint operator + ( BasicPoint l,const BasicPoint & r ){ l+=r; return l; }

    friend BasicPoint operator / ( BasicPoint l,const T r ){ l/=r; return l; }
    friend BasicPoint operator * ( BasicPoint l,const T r ){ l*=r; return l; }

    BasicPoint operator - () const { return BasicPoint(-x,-y); }

    T manhattanLength() const { return std::sqrt(x*x+y*y); }
    T quadLength()      const { return x*x+y*y; }

    bool operator ==( const BasicPoint & other ) const { return x==other.x && y==other.y; }
    bool operator !=( const BasicPoint & other ) const { return x!=other.x || y!=other.y; }

    T x=T();
    T y=T();
  };

template<class T>
class BasicPoint<T,3> {
  public:
    BasicPoint()=default;
    BasicPoint(T x,T y,T z):x(x),y(y),z(z){}

    BasicPoint& operator -= ( const BasicPoint & p ){ x-=p.x; y-=p.y; z-=p.z; return *this; }
    BasicPoint& operator += ( const BasicPoint & p ){ x+=p.x; y+=p.y; z+=p.z; return *this; }

    BasicPoint& operator /= ( const T& p ){ x/=p; y/=p; z/=p; return *this; }
    BasicPoint& operator *= ( const T& p ){ x*=p; y*=p; z*=p; return *this; }

    friend BasicPoint operator - ( BasicPoint l,const BasicPoint & r ){ l-=r; return l; }
    friend BasicPoint operator + ( BasicPoint l,const BasicPoint & r ){ l+=r; return l; }

    friend BasicPoint operator / ( BasicPoint l,const T r ){ l/=r; return l; }
    friend BasicPoint operator * ( BasicPoint l,const T r ){ l*=r; return l; }

    BasicPoint operator - () const { return BasicPoint(-x,-y,-z); }

    T manhattanLength() const { return std::sqrt(x*x+y*y+z*z); }
    T quadLength()      const { return x*x+y*y+z*z; }

    bool operator ==( const BasicPoint & other ) const { return x==other.x && y==other.y && z==other.z; }
    bool operator !=( const BasicPoint & other ) const { return x!=other.x || y!=other.y || z!=other.z; }

    T x=T();
    T y=T();
    T z=T();
  };

template<class T>
struct BasicSize {
  explicit BasicSize(T is = T()):w(is), h(is) {}
  BasicSize(T ix, T iy):w(ix), h(iy) {}

  T w=T(), h=T();

  BasicPoint<T,2> toPoint() const { return BasicPoint<T,2>{w,h};      }
  BasicRect<T>    toRect()  const { return BasicRect<T>{T(),T(),w,h}; }
  bool            isEmpty() const { return w<=0 || h<=0;              }

  bool operator ==( const BasicSize & other ) const { return w==other.w && h==other.h; }
  bool operator !=( const BasicSize & other ) const { return w!=other.w || h!=other.h; }
  };

template<class T>
struct BasicRect {
  BasicRect()=default;
  BasicRect( T ix,
             T iy,
             T iw,
             T ih )
    :x(ix), y(iy), w(iw), h(ih) {}
  T x=T(), y=T(), w=T(), h=T();

  BasicPoint<T,2> pos()  const { return BasicPoint<T,2>{x,y}; }
  BasicSize<T>    size() const { return BasicSize<T>(w,h);    }

  BasicRect intersected(const BasicRect& r) const {
    BasicRect re;
    re.x = std::max( x, r.x );
    re.y = std::max( y, r.y );

    re.w = std::min( x+w, r.x+r.w ) - re.x;
    re.h = std::min( y+h, r.y+r.h ) - re.y;

    re.w = std::max(0, re.w);
    re.h = std::max(0, re.h);

    return re;
    }

  bool contains( const BasicPoint<T,2> & p ) const {
    return contains(p.x, p.y);
    }
  bool contains( const T& px,const T& py ) const {
    return ( x<px && px<x+w ) && ( y<py && py<y+h );
    }

  bool contains( const BasicPoint<T,2> & p, bool border ) const {
    return contains(p.x, p.y,border);
    }
  bool contains( const T& px,const T& py, bool border ) const {
    if( border )
      return ( x<=px && px<=x+w ) && ( y<=py && py<=y+h ); else
      return contains(x,y);
    }

  bool isEmpty() const { return w<=0 || h<=0; }

  bool operator ==( const BasicRect& other ) const { return x==other.x && y==other.y && w==other.w && h==other.h; }
  bool operator !=( const BasicRect& other ) const { return x!=other.x || y!=other.y || w!=other.w || h!=other.h; }
  };

using Point =BasicPoint<int,2>;
using PointF=BasicPoint<float,2>;

using Vec1 =BasicPoint<float,1>;
using Vec2 =BasicPoint<float,2>;
using Vec3 =BasicPoint<float,3>;

using Size=BasicSize<int>;
using Rect=BasicRect<int>;
/*
class Rect:public BasicRect<int> {
  public:
    using BasicRect<int>::BasicRect;
  };*/
};
