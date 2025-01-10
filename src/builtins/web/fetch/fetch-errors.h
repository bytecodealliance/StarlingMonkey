#ifndef FETCH_ERRORS_H
#define FETCH_ERRORS_H

#include "builtin.h"

namespace FetchErrors {
DEF_ERR(FetchNetworkError, JSEXN_TYPEERR, "NetworkError when attempting to fetch resource", 0)
DEF_ERR(InvalidRespondWithArg, JSEXN_TYPEERR, "FetchEvent#respondWith must be called with a Response "
                           "object or a Promise resolving to a Response object as "
                           "the first argument", 0)
DEF_ERR(InvalidInitArg, JSEXN_TYPEERR, "{0}: |init| parameter can't be converted to a dictionary", 1)
DEF_ERR(NonBodyRequestWithBody, JSEXN_TYPEERR, "Request constructor: HEAD or GET Request cannot have a body", 0)
DEF_ERR(NonBodyResponseWithBody, JSEXN_TYPEERR, "Response constructor: response with status {0} cannot have a body", 1)
DEF_ERR(BodyStreamUnusable, JSEXN_TYPEERR, "Can't use a ReadableStream that's locked or has ever been read from or canceled", 0)
DEF_ERR(BodyStreamTeeingFailed, JSEXN_ERR, "Cloning body stream failed", 0)
DEF_ERR(InvalidStatus, JSEXN_RANGEERR, "{0}: invalid status {1}", 2)
DEF_ERR(InvalidStreamChunk, JSEXN_TYPEERR, "ReadableStream used as a Request or Response body must produce Uint8Array values", 0)
DEF_ERR(EmptyHeaderName, JSEXN_TYPEERR, "{0}: Header name can't be empty", 1)
DEF_ERR(InvalidHeaderName, JSEXN_TYPEERR, "{0}: Invalid header name \"{1}\"", 2)
DEF_ERR(InvalidHeaderValue, JSEXN_TYPEERR, "{0}: Invalid header value \"{1}\"", 2)
DEF_ERR(HeadersCloningFailed, JSEXN_ERR, "Failed to clone headers", 0)
DEF_ERR(HeadersImmutable, JSEXN_TYPEERR, "{0}: Headers are immutable", 1)
};     // namespace FetchErrors

#endif // FETCH_ERRORS_H
