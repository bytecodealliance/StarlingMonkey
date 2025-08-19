#ifndef BUILTINS_WEB_FORM_DATA_PARSER_
#define BUILTINS_WEB_FORM_DATA_PARSER_

#include "builtin.h"
#include "form-data.h"

namespace builtins::web::form_data {

class FormDataParser {
public:
  virtual JSObject *parse(JSContext *cx, std::string_view body) = 0;

  FormDataParser() = default;
  virtual ~FormDataParser() = default;

  FormDataParser(const FormDataParser &) = default;
  FormDataParser(FormDataParser &&) = delete;

  FormDataParser &operator=(const FormDataParser &) = default;
  FormDataParser &operator=(FormDataParser &&) = delete;

  static std::unique_ptr<FormDataParser> create(std::string_view content_type);
};

} // namespace builtins::web::form_data

#endif // BUILTINS_WEB_FORM_DATA_PARSER_
