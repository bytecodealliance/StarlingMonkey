#ifndef CORE_ERRORS_H
#define CORE_ERRORS_H

bool throw_error(JSContext* cx, const JSErrorFormatString &error,
                 const char* arg1 = nullptr,
                 const char* arg2 = nullptr,
                 const char* arg3 = nullptr,
                 const char* arg4 = nullptr);

namespace Errors {
DEF_ERR(InvalidSequence, JSEXN_TYPEERR, "Failed to construct {0} object. If defined, the first "
                     "argument must be either a [ ['name', 'value'], ... ] sequence, "
                     "or a { 'name' : 'value', ... } record{1}.", 2)
DEF_ERR(ForEachCallback, JSEXN_TYPEERR, "Failed to execute 'forEach' on '{0}': "
                                        "parameter 1 is not of type 'Function'", 1)
DEF_ERR(RequestHandlerOnly, JSEXN_TYPEERR, "{0} can only be used during request handling, "                             \
                                           "not during initialization", 1)
DEF_ERR(InitializationOnly, JSEXN_TYPEERR, "{0} can only be used during request handling, "
                                           "not during initialization", 1)
};     // namespace Errors

#endif // CORE_ERRORS_H
