#pragma once

#include <cstdint>

namespace Tempest {
  class WidgetState final {
    public:
      enum ButtonType : uint8_t {
        T_PushButton      = 0,
        T_ToolButton      = 1,
        T_FlatButton      = 2,
        T_CheckableButton = 3
        };

      enum CheckState : uint8_t {
        Unchecked        = 0,
        Checked          = 1,
        PartiallyChecked = 2
        };

      enum EchoMode : uint8_t {
        Normal   = 0,
        NoEcho   = 1,
        Password = 2
        };

      // all widgets
      bool disabled       = false;
      bool focus          = false;
      bool moveOver       = false;
      bool visible        = true;

      // click-controls
      ButtonType button   = T_PushButton;
      bool       pressed  = false;
      CheckState checked  = Unchecked;

      // editable
      bool       editable = true;
      EchoMode   echo     = Normal;
    };
  }
