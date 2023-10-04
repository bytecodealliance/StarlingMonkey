#ifndef JS_COMPUTE_RUNTIME_SEQUENCE_HPP
#define JS_COMPUTE_RUNTIME_SEQUENCE_HPP

// TODO: remove these once the warnings are fixed
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
#include "jsapi.h"
#include "jsfriendapi.h"
#include "js/ForOfIterator.h"
#pragma clang diagnostic pop

namespace core {

inline bool report_sequence_or_record_arg_error(JSContext *cx, const char *name,
                                                const char *alt_text) {
  JS_ReportErrorUTF8(cx,
                     "Failed to construct %s object. If defined, the first "
                     "argument must be either a [ ['name', 'value'], ... ] sequence, "
                     "or a { 'name' : 'value', ... } record%s.",
                     name, alt_text);
  return false;
}

/**
 * Extract <key,value> pairs from the given value if it is either a
 * sequence<sequence<Value> or a record<Value, Value>.
 */
template <auto apply>
bool maybe_consume_sequence_or_record(JSContext *cx, JS::HandleValue initv, JS::HandleObject target,
                                      bool *consumed, const char *ctor_name,
                                      const char *alt_text = "") {
  if (initv.isUndefined()) {
    *consumed = true;
    return true;
  }

  JS::RootedValue key(cx);
  JS::RootedValue value(cx);

  // First, try consuming args[0] as a sequence<sequence<Value>>.
  JS::ForOfIterator it(cx);
  if (!it.init(initv, JS::ForOfIterator::AllowNonIterable))
    return false;

  // Note: this currently doesn't treat strings as iterable even though they
  // are. We don't have any constructors that want to iterate over strings, and
  // this makes things a lot easier.
  if (initv.isObject() && it.valueIsIterable()) {
    JS::RootedValue entry(cx);

    while (true) {
      bool done;
      if (!it.next(&entry, &done))
        return false;

      if (done)
        break;

      if (!entry.isObject())
        return report_sequence_or_record_arg_error(cx, ctor_name, alt_text);

      JS::ForOfIterator entr_iter(cx);
      if (!entr_iter.init(entry, JS::ForOfIterator::AllowNonIterable))
        return false;

      if (!entr_iter.valueIsIterable())
        return report_sequence_or_record_arg_error(cx, ctor_name, alt_text);

      {
        bool done;

        // Extract key.
        if (!entr_iter.next(&key, &done))
          return false;
        if (done)
          return report_sequence_or_record_arg_error(cx, ctor_name, alt_text);

        // Extract value.
        if (!entr_iter.next(&value, &done))
          return false;
        if (done)
          return report_sequence_or_record_arg_error(cx, ctor_name, alt_text);

        // Ensure that there aren't any further entries.
        if (!entr_iter.next(&entry, &done))
          return false;
        if (!done)
          return report_sequence_or_record_arg_error(cx, ctor_name, alt_text);

        if (!apply(cx, target, key, value, ctor_name))
          return false;
      }
    }
    *consumed = true;
  } else if (initv.isObject()) {
    // init isn't an iterator, so if it's an object, it must be a record to be
    // valid input.
    JS::RootedObject init(cx, &initv.toObject());
    JS::RootedIdVector ids(cx);
    if (!js::GetPropertyKeys(cx, init, JSITER_OWNONLY | JSITER_SYMBOLS, &ids))
      return false;

    JS::RootedId curId(cx);
    for (size_t i = 0; i < ids.length(); ++i) {
      curId = ids[i];
      key = js::IdToValue(curId);

      if (!JS_GetPropertyById(cx, init, curId, &value))
        return false;

      if (!apply(cx, target, key, value, ctor_name))
        return false;
    }
    *consumed = true;
  } else {
    *consumed = false;
  }

  return true;
}

}

#endif
