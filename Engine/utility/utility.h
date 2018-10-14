#pragma once

#include <cstdint>
#include <cmath>

namespace Tempest {

template<class T,size_t len>
class BasicPoint;

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

using Point=BasicPoint<int,2>;
using Vec1 =BasicPoint<float,1>;
using Vec2 =BasicPoint<float,2>;
//using vec3 =BasicPoint<float,3>;
};
