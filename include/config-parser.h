#ifndef CONFIG_PARSER_H
#define CONFIG_PARSER_H

#include "extension-api.h"
#include <string_view>

#include <iostream>

namespace starling {
class ConfigParser {
  static constexpr const char* DEFAULT_SCRIPT_PATH = "./index.js";
  std::unique_ptr<api::EngineConfig> config_;

public:
  ConfigParser() : config_(std::make_unique<api::EngineConfig>()) {
    config_->content_script_path = DEFAULT_SCRIPT_PATH;
  }

  /**
   * Read configuration from a given environment variable.
   *
   * The variable's contents are expected to be in the format of a command line, minus the
   * program name, so all the examples for the `apply_args` method apply here, too.
   */
  ConfigParser *apply_env(std::string_view envvar_name = "STARLINGMONKEY_CONFIG") {
    if (const char *config = std::getenv(envvar_name.data())) {
      return apply_args(config);
    }

    return this;
  }

  /**
  * Split the given string into arguments and apply them to the configuration.
   *
   * The string contents are expected to be in the format of a command line, minus the
   * program name, so all the examples for the other `apply_args` overload apply here, too.
   */
  ConfigParser *apply_args(std::string_view args_string) {
    std::vector<std::string_view> args = { "starling-raw.wasm" };
    char last = '\0';
    bool in_quotes = false;
    size_t slice_start = 0;
    for (size_t i = 0; i < args_string.size(); i++) {
      char c = args_string[i];

      if ((!in_quotes && isspace(c)) || (c == '"' && last != '\\')) {
        if (slice_start < i) {
          args.push_back(args_string.substr(slice_start, i - slice_start));
        }
        slice_start = i + 1;
      }
      if (c == '"' && last != '\\') {
        in_quotes = !in_quotes;
      }
      last = c;
    }

    if (slice_start < args_string.size()) {
      args.push_back(args_string.substr(slice_start));
    }

    return apply_args(args);
  }

  /**
   * Parse the given arguments and apply them to the configuration.
   *
   * Can be called multiple times, with the values set in the last call taking precedence over
   * values that might have been set in previous calls, including indirectly through `apply_env`.
   */
  ConfigParser *apply_args(std::vector<std::string_view> args) {
    for (size_t i = 1; i < args.size(); i++) {
      if (args[i] == "-e" || args[i] == "--eval") {
        if (i + 1 < args.size()) {
          config_->content_script = args[i + 1];
          config_->content_script_path = nullptr;
          i++;
        }
      } else if (args[i] == "-i" || args[i] == "--initializer-script-path") {
        if (i + 1 < args.size()) {
          config_->initializer_script_path = args[i + 1];
          i++;
        }
      } else if (args[i] == "-v" || args[i] == "--verbose") {
        config_->verbose = true;
      } else if (args[i] == "-d" || args[i] == "--enable-script-debugging") {
        config_->debugging = true;
      } else if (args[i] == "--strip-path-prefix") {
        if (i + 1 < args.size()) {
          config_->path_prefix = args[i + 1];
          i++;
        }
      } else if (args[i] == "--legacy-script") {
        config_->module_mode = false;
        if (i + 1 < args.size()) {
          config_->content_script_path = args[i + 1];
          i++;
        }
      } else if (args[i] == "--wpt-mode") {
        config_->wpt_mode = true;
      } else if (args[i].starts_with("--")) {
        std::cerr << "Unknown option: " << args[i] << std::endl;
        exit(1);
      } else {
        config_->content_script_path = args[i];
      }
    }

    return this;
  }

  /**
   * Take the configuration object.
   *
   * This method is meant to be called after all the desired configuration has been applied.
   */
  std::unique_ptr<api::EngineConfig> take() { return std::move(config_); }
};

}

#endif // CONFIG_PARSER_H
