#ifndef WASI_PREVIEW2_EXPORTS
#define WASI_PREVIEW2_EXPORTS

#define exports_wasi_http_incoming_handler exports_wasi_http_incoming_handler_handle
#define exports_wasi_http_incoming_request                                                         \
  exports_wasi_http_incoming_handler_own_incoming_request_t
#define exports_wasi_http_response_outparam                                                        \
  exports_wasi_http_incoming_handler_own_response_outparam_t

#endif
