
# Build constants ##############################################################


# The path to the directory containing this Makefile.
ROOT := $(shell dirname "$(realpath $(firstword $(MAKEFILE_LIST)))")

# The path to the //runtime directory.
RT_SRC := $(ROOT)/runtime
CPP_SRC := $(ROOT)/cpp

# The name of the project
PROJECT_NAME := "WebAssembly JS Runtime"
PROJECT_VERSION := "0.1.0"

# Environmentally derived config ###############################################

# Build verbosity, useful when debugging build failures. Setting it to anything
# will cause the quiet output to be disabled.
V ?=

# The destination directory when installing the resulting wasm binaries.
DESTDIR ?= .

# The path to the wasi-sdk installation.
WASI_SDK ?= /opt/wasi-sdk

# The version of OpenSSL to build with.
OPENSSL_VERSION = 3.0.7

# Whether or not this will be a debug build. When set to anything other than
# `false` the build will be treated as a debug build.
DEBUG ?= false

# The path to the wit-bindgen executable
WIT_BINDGEN ?= $(shell which wit-bindgen)

# Default optimization flgs for clang/clang++.
OPT_FLAGS ?= -O2

# Command helpers for making nice build output.
include mk/commands.mk

# Derived configuration ########################################################

ifneq ($(CAE),)
HOST_INTERFACE := $(ROOT)/compute-at-edge
else
HOST_INTERFACE :=$(ROOT)/wasi-0.2.0-rc-2023-10-18
endif

# The wasi-sdk provided c++ compiler wrapper.
WASI_CXX ?= $(WASI_SDK)/bin/clang++

# The wasi-sdk provided c compiler wrapper.
WASI_CC ?= $(WASI_SDK)/bin/clang

# The wasi-sdk provided ar wrapper.
WASI_AR ?= $(WASI_SDK)/bin/ar

TEST_JS ?= $(ROOT)/tests/smoke.js

ifneq ($(DEBUG),false)
  MODE := debug
  CARGO_FLAG :=
  OPT_FLAGS += -DDEBUG -DJS_DEBUG -g
  ADAPTER_DIR := $(HOST_INTERFACE)/preview1-adapter-debug

  # Define an empty WASM_STRIP macro when making a debug build
  WASM_STRIP =
else
  MODE := release
  CARGO_FLAG := --release
  ADAPTER_DIR := $(HOST_INTERFACE)/preview1-adapter-release

  # Strip binaries when making a non-debug build.
  WASM_STRIP = wasm-opt --strip-debug -o $1 $1
endif

ADAPTER := $(ADAPTER_DIR)/wasi_snapshot_preview1.wasm

# The path to the wasm-tools executable
WASM_TOOLS ?= $(shell which wasm-tools)

ifeq ($(WASM_TOOLS),)
$(error ERROR: "No wasm-tools found in PATH, consider running 'cargo install wasm-tools'")
else
WASM_METADATA = $(WASM_TOOLS) metadata add --sdk $(PROJECT_NAME)=$(PROJECT_VERSION) --output $1 $1
COMPONENT_TYPE = $(WASM_TOOLS) component new --adapt $(ADAPTER) --output $1 $1
endif

# The path to the wizer executable
WIZER ?= $(shell which wizer)

ifeq ($(WIZER),)
$(error ERROR: "No wizer found in PATH, consider running 'cargo install wizer --features=\"env_logger structopt\"")
else
WIZEN = echo $2 | WASMTIME_BACKTRACE_DETAILS=1 $(WIZER) --allow-wasi --wasm-bulk-memory true --inherit-stdio true --dir $(ROOT) -o $1 -- $3
endif

# The base build directory, where all our build artifacts go.
BUILD := $(ROOT)/build

# The output directory for the current build mode (release/debug).
OBJ_DIR := $(BUILD)/$(MODE)

# The path to the //runtime/spidermonkey/$(MODE) directory.
SM_SRC := $(ROOT)/deps/spidermonkey/$(MODE)

# The objects we link in from spidermonkey
SM_OBJ := $(wildcard $(SM_SRC)/lib/*.o)
SM_OBJ += $(wildcard $(SM_SRC)/lib/*.a)

# This is required when using spidermonkey headers, as it allows us to enable
# the streams library when setting up the js context.
DEFINES := -DMOZ_JS_STREAMS

BINDINGS := $(HOST_INTERFACE)/bindings

# Flags for c++ compilation
CXX_FLAGS := -std=gnu++20 -Wall -Werror -Qunused-arguments
CXX_FLAGS += -fno-sized-deallocation -fno-aligned-new -mthread-model single
CXX_FLAGS += -fPIC -fno-rtti -fno-exceptions -fno-math-errno -pipe
CXX_FLAGS += -fno-omit-frame-pointer -funwind-tables -m32
CXX_FLAGS += --sysroot=$(WASI_SDK)/share/wasi-sysroot

# Flags for C compilation
CFLAGS := -Wall -Werror -Wno-unknown-attributes -Wno-pointer-to-int-cast
CFLAGS += -Wno-int-to-pointer-cast -m32
CFLAGS += --sysroot=$(WASI_SDK)/share/wasi-sysroot

# Includes for compiling c++
INCLUDES := -I$(RT_SRC)
INCLUDES += -I$(CPP_SRC)
INCLUDES += -I$(CPP_SRC)/host_interface
INCLUDES += -I$(SM_SRC)/include
INCLUDES += -I$(ROOT)/deps/include
INCLUDES += -I$(ROOT)/deps/fmt/include
INCLUDES += -I$(ROOT)/crates
INCLUDES += -I$(BINDINGS)
INCLUDES += -I$(BUILD)/openssl/include
# Appease VSCode on macos, which otherwise thinks the APPLE define is set.
INCLUDES += -include $(ROOT)/.vscode/vscode-preinclude.h

# Linker flags.
LD_FLAGS := -Wl,-z,stack-size=1048576 -Wl,--stack-first
LD_FLAGS += -mexec-model=reactor
LD_FLAGS += -lwasi-emulated-signal
LD_FLAGS += -lwasi-emulated-process-clocks
LD_FLAGS += -lwasi-emulated-getpid
LD_FLAGS += -L$(BUILD)/openssl/libx32 -lcrypto


# Default targets ##############################################################

.PHONY: all
all: $(BUILD)/js-component.wasm

# Remove just the build artifacts for the current runtime build.
.PHONY: clean
clean:
	$(call cmd,rm,$(BUILD)/js-runtime.wasm)
	$(call cmd,rmdir,$(BUILD)/release)
	$(call cmd,rmdir,$(BUILD)/debug)

# Remove all build artifacts.
.PHONY: distclean
distclean: clean
	$(call cmd,rmdir,$(BUILD))

# Run clang-format over the codebase.
.PHONY: format
format: $(CPP_FILES)
	$(ROOT)/../ci/clang-format.sh --fix


# Build directories ############################################################

$(BUILD):
	$(call cmd,mkdir,$@)

$(OBJ_DIR):
	$(call cmd,mkdir,$@)

$(OBJ_DIR)/deps/fmt/src:
	$(call cmd,mkdir,$@)

shared:
	$(call cmd,mkdir,$@)

# Downloaded dependencies ######################################################

$(BUILD)/openssl-$(OPENSSL_VERSION).tar.gz: URL=https://www.openssl.org/source/openssl-$(OPENSSL_VERSION).tar.gz
$(BUILD)/openssl-$(OPENSSL_VERSION).tar.gz: | $(BUILD)
	$(call cmd,wget,$@)

# OpenSSL build ################################################################

# Convenience target for building openssl.
.PHONY: openssl
openssl: $(BUILD)/openssl/token

# Extract and prepare the openssl build directory.
$(BUILD)/openssl-$(OPENSSL_VERSION)/token: $(BUILD)/openssl-$(OPENSSL_VERSION).tar.gz $(ROOT)/deps/patches/getuid.patch $(ROOT)/deps/patches/rand.patch
	$Q tar -C $(BUILD) -xf $<
	$Q patch -d $(BUILD)/openssl-$(OPENSSL_VERSION) -p1 < $(ROOT)/deps/patches/getuid.patch
	$Q patch -d $(BUILD)/openssl-$(OPENSSL_VERSION) -p1 < $(ROOT)/deps/patches/rand.patch
	$Q touch $@

OPENSSL_OPTS := -static -no-sock -no-asm -no-ui-console -no-egd
OPENSSL_OPTS += -no-afalgeng -no-tests -no-stdio -no-threads
OPENSSL_OPTS += -D_WASI_EMULATED_SIGNAL
OPENSSL_OPTS += -D_WASI_EMULATED_PROCESS_CLOCKS
OPENSSL_OPTS += -D_WASI_EMULATED_GETPID
OPENSSL_OPTS += -DHAVE_FORK=0
OPENSSL_OPTS += -DNO_SYSLOG
OPENSSL_OPTS += -DNO_CHMOD
OPENSSL_OPTS += -DOPENSSL_NO_SECURE_MEMORY
OPENSSL_OPTS += --with-rand-seed=getrandom
OPENSSL_OPTS += --prefix=$(BUILD)/openssl
OPENSSL_OPTS += --cross-compile-prefix=$(WASI_SDK)/bin/
OPENSSL_OPTS += linux-x32

OPENSSL_DISABLED_WARNINGS := -Wno-unused-command-line-argument
OPENSSL_DISABLED_WARNINGS += -Wno-constant-conversion
OPENSSL_DISABLED_WARNINGS += -Wno-shift-count-overflow

# Configure and build openssl.
$(BUILD)/openssl/token: $(BUILD)/openssl-$(OPENSSL_VERSION)/token
	$Q export WASI_SDK_PATH=$(WASI_SDK) && \
		cd $(BUILD)/openssl-$(OPENSSL_VERSION) && \
		CC=clang \
		CFLAGS="--sysroot=$(WASI_SDK)/share/wasi-sysroot" \
		./Configure $(OPENSSL_OPTS) && \
		$(MAKE) -j8 && \
		$(MAKE) install_sw
	$Q touch $@


# rusturl build ################################################################

RUST_URL_SRC := $(ROOT)/crates/rust-url

RUST_URL_RS_FILES := $(shell find $(RUST_URL_SRC)/src -name '*.rs')

RUST_URL_LIB := $(BUILD)/rusturl/wasm32-wasi/$(MODE)/librust_url.a

rusturl: $(RUST_URL_LIB)

$(RUST_URL_LIB): $(RUST_URL_RS_FILES)
$(RUST_URL_LIB): $(RUST_URL_SRC)/Cargo.toml
$(RUST_URL_LIB): $(RUST_URL_SRC)/cbindgen.toml
$(RUST_URL_LIB): | $(BUILD)
	$(call cmd_format,CARGO,$@) \
	cd $(RUST_URL_SRC) && cbindgen --output rust-url.h && \
	cargo build $(call quiet_flag,--quiet) \
		--manifest-path $(RUST_URL_SRC)/Cargo.toml \
		--target-dir $(BUILD)/rusturl \
		--target=wasm32-wasi $(CARGO_FLAG)

# rustencoding build ################################################################

RUST_ENCODING_SRC := $(ROOT)/crates/rust-encoding

RUST_ENCODING_RS_FILES := $(shell find $(RUST_ENCODING_SRC)/src -name '*.rs')

RUST_ENCODING_LIB := $(BUILD)/rustencoding/wasm32-wasi/$(MODE)/librust_encoding.a

rustencoding: $(RUST_ENCODING_LIB)

$(RUST_ENCODING_LIB): $(RUST_ENCODING_RS_FILES)
$(RUST_ENCODING_LIB): $(RUST_ENCODING_SRC)/Cargo.toml
$(RUST_ENCODING_LIB): $(RUST_ENCODING_SRC)/cbindgen.toml
$(RUST_ENCODING_LIB): | $(BUILD)
	$(call cmd_format,CARGO,$@) \
	cd $(RUST_ENCODING_SRC) && cbindgen --output rust-encoding.h && \
	cargo build $(call quiet_flag,--quiet) \
		--manifest-path $(RUST_ENCODING_SRC)/Cargo.toml \
		--target-dir $(BUILD)/rustencoding \
		--target=wasm32-wasi $(CARGO_FLAG)


# wit-bindgen integration ######################################################

.PHONY: regenerate-world
ifeq ($(WIT_BINDGEN),)
regenerate-world:
	@echo ""
	@echo "No wit-bindgen found in PATH, consider running"
	@echo ""
	@echo "  cargo install wit-bindgen"
	@echo ""
	@exit 1
else
regenerate-world:
	$Q $(WIT_BINDGEN) c \
	  --out-dir $(BINDINGS) \
	  --world bindings \
	  $(HOST_INTERFACE)/wit
endif


# JS runtime shared build #################################################

CPP_FILES := $(wildcard $(RT_SRC)/*.cpp)
CPP_FILES += $(shell find $(HOST_INTERFACE) -type f -name '*.cpp')
CPP_FILES += $(shell find $(CPP_SRC) -type f -name '*.cpp')
CPP_FILES += $(shell find $(CPP_SRC) -type f -name '*.cc')
CPP_FILES += $(ROOT)/deps/fmt/src/format.cc
CPP_OBJ := $(call build_dest,$(call change_src_extension,$(CPP_FILES),o))


# Build all the above object files
$(foreach source,$(CPP_FILES),$(eval $(call compile_cxx,$(source))))

# JS runtime component build ##############################################

BINDINGS_SRC := $(shell find $(BINDINGS) -type f -name '*.c')
BINDINGS_OBJ := $(call build_dest,$(call change_src_extension,$(BINDINGS_SRC),o))
$(foreach source,$(BINDINGS_SRC),$(eval $(call compile_c,$(source))))

# NOTE: we shadow wasm-opt by adding $(ROOT)/scripts to the path, which
# includes a script called wasm-opt that immediately exits successfully. See
# that script for more information about why we do this.
$(OBJ_DIR)/js-runtime.wasm: $(CPP_OBJ) $(SM_OBJ) $(RUST_URL_LIB) $(RUST_ENCODING_LIB)
$(OBJ_DIR)/js-runtime.wasm: $(BINDINGS)/*.o $(BINDINGS_OBJ)
	$(call cmd_format,WASI_LD,$@) PATH="$(ROOT)/scripts:$$PATH" \
	$(WASI_CXX) $(LD_FLAGS) $(OPENSSL_LIBS) -o $@ $^
	$(call cmd_format,WASM_STRIP,$@) $(call WASM_STRIP,$@)
	$(call cmd_format,WASM_METADATA,$@) $(call WASM_METADATA,$@)

# Shared builtins build ########################################################

.PHONY: shared-builtins
shared-builtins: shared.a

.PHONY: shared.a
shared.a: $(OBJ_DIR)/shared.a
	$(call cmd,cp,$@)

extract_lib = $(call cmd_format,WASI_AR [x],$2) $(WASI_AR) -x --output $1 $2

$(OBJ_DIR)/shared: $(OBJ_DIR)/builtins.a $(RUST_URL_LIB) $(RUST_ENCODING_LIB)
	$(call cmd,mkdir,$@)
	$(call extract_lib,$(OBJ_DIR)/shared,$(OBJ_DIR)/builtins.a)
	$(call extract_lib,$(OBJ_DIR)/shared,$(RUST_URL_LIB))
	$(call extract_lib,$(OBJ_DIR)/shared,$(RUST_ENCODING_LIB))

$(OBJ_DIR)/shared.a: | $(OBJ_DIR)/shared
	$(call cmd,wasi_ar,$(wildcard $(OBJ_DIR)/shared/*.o))

$(OBJ_DIR)/builtins.a: $(filter $(OBJ_DIR)/builtins/shared/%.o,$(CPP_OBJ))
$(OBJ_DIR)/builtins.a: $(OBJ_DIR)/builtin.o
$(OBJ_DIR)/builtins.a: $(OBJ_DIR)/core/encode.o
	$(call cmd,wasi_ar,$^)

# This rule copies the built artifact into the $(BUILD) directory, and
# is marked phony as we need to do the right thing when running the
# following sequence:
#
# make; DEBUG=1 make; make
#
# Without marking it phony, the wasm won't be copied in the last invocation of
# make, as it will look up-to-date.

.PHONY: $(BUILD)/js-component.wasm
$(BUILD)/js-component.wasm: $(OBJ_DIR)/js-runtime.wasm $(TEST_JS)
	$(call cmd_format,WIZEN,$@, $(TEST_JS)) $(call WIZEN,$@, $(TEST_JS), $(OBJ_DIR)/js-runtime.wasm)
	$(call cmd_format,COMPONENT_TYPE,$@) $(call COMPONENT_TYPE,$@)


# Debugging rules ##############################################################

# Useful for debugging, try `make print-CPP_FILES`
print-%:
	$Q echo "$* = '$($*)'"


# Development rules ############################################################

# Generate a compile_commands.json for powering clangd.
.PHONY: compile_commands.json
compile_commands.json: .vscode/compile_commands.json
.vscode/compile_commands.json: $(CPP_FILES)
	$Q ( \
		sep="["; \
		for file in $(CPP_FILES); do \
			echo "$$sep"; \
			sep=","; \
			echo "{ \"directory\": \"$(ROOT)\","; \
			echo "  \"command\": \"$(WASI_CXX) $(CXX_FLAGS) $(INCLUDES) $(DEFINES)\","; \
			echo "  \"file\": \"$${file}\"}"; \
		done; \
		echo; \
		echo ']' \
	) > "$@"
