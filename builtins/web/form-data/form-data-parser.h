#ifndef BUILTINS_WEB_FORM_DATA_PARSER_
#define BUILTINS_WEB_FORM_DATA_PARSER_

#include "builtin.h"
#include "form-data.h"

namespace builtins {
namespace web {
namespace form_data {

class FormDataParser {
public:
  virtual JSObject *parse(JSContext *cx, std::string_view body) = 0;
  virtual ~FormDataParser() = default;

  static std::unique_ptr<FormDataParser> create(std::string_view content_type);
};

} // namespace form_data
} // namespace web
} // namespace builtins

#endif // BUILTINS_WEB_FORM_DATA_PARSER_
