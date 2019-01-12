#pragma once

#include <vector>
#include <memory>

namespace Tempest {

class UndoStack {
  public:
    UndoStack();
    virtual ~UndoStack()=default;

    class Command {
      public:
        virtual ~Command()=default;

        virtual void redo()=0;
        virtual void undo()=0;
      };

    void push(Command* cmd);
    void undo();
    void redo();

  private:
    std::vector<std::unique_ptr<Command>> stk, undoStk;
  };

}
