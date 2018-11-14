#pragma once

#include <cstring>

namespace Tempest{

  //! Цвет, rgba, [0..1], одинарная точность.
  class Color {
    public:
      //! Констуктор. Эквивалентно Color(0.0).
      Color()=default;

      //! Тоже конструктор. Всем каналам будет задано значение rgba.
      Color( float rgba ){ cdata[0]=rgba;cdata[1]=rgba;cdata[2]=rgba;cdata[3]=rgba; }

      //! Конструктор.
      Color( float r, float g,
             float b, float a = 1.0 ){cdata[0]=r;cdata[1]=g;cdata[2]=b;cdata[3]=a;}
      Color(const Color&)=default;

      //! Присваивание.
      Color& operator = ( const Color & other)=default;


      //! Задание всех каналов явно.
      void set(float r,
               float g,
               float b,
               float a){
        cdata[0]=r;cdata[1]=g;cdata[2]=b;cdata[3]=a;
        }
      //! Перегруженная функция, введена для удобства.
      void set( float rgba ){
        cdata[0]=rgba;cdata[1]=rgba;cdata[2]=rgba;cdata[3]=rgba;
        }

      //! Получить 4х элементный массив,
      //! элементы которого являются значениями каналов.
      const float * data() const { return cdata; }

      //! Сложение. Результат не будет обрубаться по диапозону.
      friend Color operator + ( Color l,const Color & r ){ l+=r; return l; }
      Color&  operator += ( const Color & other) {
        cdata[0]+=other.cdata[0];
        cdata[1]+=other.cdata[1];
        cdata[2]+=other.cdata[2];
        cdata[3]+=other.cdata[3];
        return *this;
        }
      //! Вычитание. Результат не будет обрубаться по диапозону.
      friend Color operator - ( Color l,const Color & r ){ l-=r; return l; }
      Color&  operator -= ( const Color & other) {
        cdata[0]-=other.cdata[0];
        cdata[1]-=other.cdata[1];
        cdata[2]-=other.cdata[2];
        cdata[3]-=other.cdata[3];
        return *this;
        }

      //! Канал red.
      float r() const { return cdata[0]; }
      //! Канал green.
      float g() const { return cdata[1]; }
      //! Канал blue.
      float b() const { return cdata[2]; }
      //! alpha канал.
      float a() const { return cdata[3]; }

      float& operator[]( int i )  { return cdata[i]; }
      const float& operator[]( int i ) const  { return cdata[i]; }

      bool operator == ( const Color & other ) const {
        return std::memcmp(this,&other,sizeof(*this))==0;
        }

      bool operator !=( const Color & other ) const {
        return std::memcmp(this,&other,sizeof(*this))!=0;
        }
    private:
      float cdata[4]={};
  };

}
