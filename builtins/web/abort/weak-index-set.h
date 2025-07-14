#ifndef WEB_BUILTINS_WEAK_INDEX_SET_
#define WEB_BUILTINS_WEAK_INDEX_SET_

#include "builtin.h"
#include "js/GCVector.h"


// Default GC policy_ for `js::GCVector<T>` is _appropriate for weak references_:
// it invokes `GCPolicy<T>::traceWeak` to drop unreachable elements automatically.
//
// TODO: Consider using `js::GCHashMap` and track insertion order separetely for
// O(1) insert/remove.
using WeakVec = JS::GCVector<JS::Heap<JSObject*>, 8, js::SystemAllocPolicy>;

class WeakIndexSet {
  WeakVec items_;

public:
  WeakIndexSet() : items_() {}

  bool insert(JSObject* obj) {
    auto it = std::find_if(items_.begin(), items_.end(), [&obj](const auto &item) { return item == obj; });

    if (it == items_.end()) {
      return items_.append(obj);
    } else {
      return true;
    }
  }

  bool remove(JSObject* obj) {
    items_.eraseIf([&obj](const auto &item) { return item == obj; });
    return true;
  }

  WeakVec &items() { return items_;}
  const WeakVec &items() const { return items_; }

  void trace(JSTracer* trc) { items_.trace(trc); }

  bool traceWeak(JSTracer* trc) { return items_.traceWeak(trc); }
};

#endif // WEB_BUILTINS_WEAK_INDEX_SET_
