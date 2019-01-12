#include "undostack.h"

using namespace Tempest;

UndoStack::UndoStack() {
  }

void UndoStack::push(UndoStack::Command *cmd) {
  cmd->redo();
  stk.emplace_back(cmd);
  }

void UndoStack::undo() {
  if(stk.size()==0)
    return;
  std::unique_ptr<Command> c = std::move(stk.back());
  stk.pop_back();
  c->undo();

  undoStk.push_back(std::move(c));
  }

void UndoStack::redo() {
  if(undoStk.size()==0)
    return;
  std::unique_ptr<Command> c = std::move(undoStk.back());
  undoStk.pop_back();
  c->redo();

  stk.push_back(std::move(c));
  }
