#pragma once

#include <Tempest/Widget>
#include <Tempest/ListDelegate>
#include <Tempest/UiOverlay>

namespace Tempest {

class ListDelegate;

class ComboBox : public Widget {
  public:
    ComboBox();
    ~ComboBox();

    Tempest::Signal<void(size_t)> onItemSelected;
    Tempest::Signal<void(size_t)> onSelectionChanged;

    void         setItems(const std::vector<std::string>& items);

    void         setDelegate(ListDelegate* d);
    void         removeDelegate();

    void         setCurrentIndex(size_t i);
    size_t       currentIndex() const;

    void         invalidateView();
    void         updateView();

  protected:
    void         paintEvent(Tempest::PaintEvent &event) override;

  private:
    struct Overlay;
    struct DropPanel;
    struct ProxyDelegate;

    template<class T>
    struct DefaultDelegate;

    size_t                        selectedId = 0;
    Overlay*                      overlay    = nullptr;

    std::unique_ptr<ListDelegate> delegate;
    ProxyDelegate*                proxyDelegate = nullptr;

    void         openMenu();
    void         closeMenu();

    void         applyItemSelection(size_t id);
    void         proxyOnItemSelected(size_t id, Widget*);
    Widget*      createDropList();
  };

}
