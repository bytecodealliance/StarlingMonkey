
# Silences command output when used in front of a command in a rule, depending
# on the value of the V env var.
Q := @

# A macro for adding quiet flags to commands, dependent on the V environment
# variable.
quiet_flag = $1

# Convenience macro for calling known commands with a nicer output format.
cmd = $(call cmd_format,$(cmd_$1_name),$@)$(call cmd_$1,$2)

# Strip off the root dir from a path.
without_root = $(subst $(ROOT)/,,$1)

# Pretty output prefix for commands
cmd_format = $Q printf '%-20s%s\n' '$1' '$(call without_root,$2)';

# When verbosity is enabled, disable all quiet/pretty output.
ifneq ($V,)
  Q :=
  quiet_flag =
  cmd_format =
endif

cmd_mkdir_name := MKDIR
cmd_mkdir = mkdir -p $1

cmd_cp_name := CP
cmd_cp = cp $< $1

cmd_rm_name := RM
cmd_rm = $(RM) $1

cmd_rmdir_name := RMDIR
cmd_rmdir = $(RM) -r $1

cmd_wget_name := WGET
cmd_wget = wget $(URL) $(call quiet_flag,--quiet) -O $1

cmd_wasi_ar_name := WASI_AR
cmd_wasi_ar = $(WASI_AR) rcs $@ $1

cmd_wasi_cxx_name := WASI_CXX
cmd_wasi_cxx = $(WASI_CXX) $(CXX_FLAGS) $(OPT_FLAGS) $(INCLUDES) $(DEFINES) -MMD -MP -c -o $1 $<

change_cxx_extension = $(patsubst %.cc,%.$2,$(1:.cpp=.$2))
change_src_extension = $(patsubst %.c,%.$2,$(patsubst %.cc,%.$2,$(1:.cpp=.$2)))

build_dest = $(patsubst $(ROOT)/%,$(OBJ_DIR)/%,$1)

# Compile a single c++ source and lazily include the dependency file.
compile_cxx = $(call compile_cxx_impl,$1,$(call build_dest,$(patsubst %.cc,%.o,$(1:.cpp=.o))))

define compile_cxx_impl

# We use shell to get the dirname of the target here, as the builtin `dir` rule
$2: $1 $(BUILD)/openssl/token
	@mkdir -p "$(shell dirname "$2")"
	$$(call cmd,wasi_cxx,$$@)

-include $(2:.o=.d)

endef

cmd_wasi_cc_name := WASI_CC
cmd_wasi_cc = $(WASI_CC) $(CFLAGS) $(OPT_FLAGS) $(DEFINES) -MMD -MP -c -o $1 $<

# Compile a single c source and lazily include the dependency file.
compile_c = $(call compile_c_impl,$1,$(call build_dest,$(1:.c=.o)))

define compile_c_impl

$2: $1
	@mkdir -p "$(shell dirname "$2")"
	$$(call cmd,wasi_cc,$$@)

-include $(2:.o=.d)

endef
