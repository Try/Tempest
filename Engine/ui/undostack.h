#pragma once

#include <vector>
#include <memory>

namespace Tempest {

template<class Subject>
class UndoStack {
  public:
    UndoStack() = default;
    virtual ~UndoStack()=default;

    class Command {
      public:
        virtual ~Command()=default;

        virtual void redo(Subject& subj)=0;
        virtual void undo(Subject& subj)=0;
      };

    void push(Subject& subj,Command* cmd);
    void undo(Subject& subj);
    void redo(Subject& subj);

  private:
    std::vector<std::unique_ptr<Command>> stk, undoStk;
  };

template<class Subject>
void UndoStack<Subject>::push(Subject& subj, Command* cmd) {
  stk.push_back(std::unique_ptr<Command>(cmd));
  try {
    stk.back()->redo(subj);
    undoStk.clear();
    }
  catch(...){
    stk.pop_back();
    throw;
    }
  }

template<class Subject>
void UndoStack<Subject>::undo(Subject& subj) {
  if(stk.size()==0)
    return;

  stk.back()->undo(subj);
  auto ptr = std::move(stk.back());
  try {
    undoStk.push_back(std::move(ptr));
    stk.pop_back();
    }
  catch(...){
    // undoStk not consistent anymore
    undoStk.clear();
    // abandon action in this case: cannot return it to stk, because it creates recursive 'fail to fail' scenario
    throw;
    }
  }

template<class Subject>
void UndoStack<Subject>::redo(Subject& subj) {
  if(undoStk.size()==0)
    return;

  stk.push_back(nullptr);
  try {
    undoStk.back()->redo(subj);
    }
  catch(...) {
    stk.pop_back();
    throw;
    }
  stk.back() = std::move(undoStk.back());
  undoStk.pop_back();
  }

}
