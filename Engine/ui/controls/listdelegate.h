#pragma once

#include <Tempest/Button>
#include <Tempest/Application>
#include <Tempest/TextCodec>

#include <cstddef>
#include <vector>
#include <sstream>
#include <cstdlib>

namespace Tempest {

class Widget;

class ListDelegate {
  public:
    enum Role : uint8_t {
      R_Default,
      R_ListBoxView,
      R_ListBoxItem,
      R_ListItem,
      R_Count
      };

    virtual ~ListDelegate(){}

    virtual size_t  size() const = 0;

    virtual Widget* createView(size_t position, Role /*role*/) { return createView(position); }
    virtual Widget* createView(size_t position               ) = 0;
    virtual void    removeView(Widget* w, size_t /*position*/) { w->deleteLater();            }

    virtual Widget* update    ( Widget* w, size_t position ){
      removeView(w,position);
      return createView(position,R_Default);
      }

    Tempest::Signal<void()>               invalidateView, updateView;
    Tempest::Signal<void(size_t)>         onItemSelected;
    Tempest::Signal<void(size_t,Widget*)> onItemViewSelected;
  };

template< class T, class VT, class Ctrl >
class AbstractListDelegate : public ListDelegate {
  public:
    AbstractListDelegate( const VT& vec ):data(vec){}

    size_t size() const override {
      return data.size();
      }

    using ListDelegate::createView;
    Widget* createView(size_t position) override {
      ListItem<Ctrl>* b = new ListItem<Ctrl>(position);
      b->onClick.bind(this,&AbstractListDelegate<T,VT,Ctrl>::implItemSelected);
      if(position<data.size())
        initializeItem(b,data[position]);
      return b;
      }

    void removeView(Widget* w, size_t position) override{
      ListDelegate::removeView(w,position);
      }

  protected:
    const VT& data;

    void initializeItem(Ctrl* c, const T& data){
      SizePolicy p = c->sizePolicy();
      p.typeH      = Preferred;
      p.maxSize.w  = SizePolicy::maxWidgetSize().w;
      c->setSizePolicy(p);

      c->setText(textOf(data));
      }

    template<class C>
    struct ListItem : C {
      ListItem(size_t id):id(id){}

      const size_t id;
      Tempest::Signal<void(size_t,Widget*)> onClick;

      void emitClick() override {
        onClick(id,this);
        }
      };

    virtual const std::string textOf( const T& t ) const = 0;

  private:
    void implItemSelected(size_t item, Widget* itemView){
      onItemSelected(item);
      onItemViewSelected(item,itemView);
      }
  };

template< class T, class Ctrl=Button >
class ArrayListDelegate : public AbstractListDelegate<T,std::vector<T>,Ctrl> {
  public:
    ArrayListDelegate( const std::vector<T>& vec ):
      AbstractListDelegate<T,std::vector<T>,Ctrl>(vec){}

  private:
    const std::string textOf( const T& t ) const override {
      return implTextOf(t);
      }

    static std::string implTextOf( const std::string& s ){
      return s;
      }

    static std::string implTextOf( const std::u16string& s ){
      return TextCodec::toUtf8(s);
      }

    template< class E >
    static std::string implTextOf( const E& s ){
      return std::to_string(s);
      }
  };
}
