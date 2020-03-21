#include "shortcut.h"

#include <Tempest/Widget>

using namespace Tempest;

Shortcut::Shortcut(Widget& w, Event::Modifier md, Event::KeyType key) {
  m.modifier = md;
  m.key      = key;
  m.owner    = &w;

  m.owner->implRegisterSCut(this);
  }

Shortcut::Shortcut(Shortcut&& sc) {
  m           = sc.m;
  onActivated = std::move(sc.onActivated);
  m.owner->implRegisterSCut(this);
  }

Shortcut::~Shortcut() {
  if(m.owner!=nullptr)
    m.owner->implUnregisterSCut(this);
  }

Shortcut &Shortcut::operator =(Shortcut&& sc ) {
  if(m.owner!=nullptr)
    m.owner->implUnregisterSCut(this);

  m = sc.m;
  onActivated = std::move(sc.onActivated);

  if(m.owner!=nullptr) {
    m.owner->implUnregisterSCut(&sc);
    sc.m = M();
    m.owner->implRegisterSCut(this);
    }
  return *this;
  }

void Shortcut::setKey(Event::KeyType key) {
  m.key  = key;
  m.lkey = 0;
  }

void Shortcut::setKey(uint32_t key) {
  m.key  = Event::K_NoKey;
  m.lkey = key;
  }

void Shortcut::setModifier(Event::Modifier mod) {
  m.modifier = mod;
  }

Event::KeyType Shortcut::key() const {
  return m.key;
  }

uint32_t Shortcut::lkey() const {
  return m.lkey;
  }

Event::Modifier Shortcut::modifier() const {
  return m.modifier;
  }
