#!/bin/sh
# Detect gsed or sed
gsed=$(type gsed >/dev/null 2>&1 && echo gsed || echo sed)
# This is one big heuristic but seems to work well enough
grep_heur() {
  grep -v "link_name" "$1" | \
  grep -v '"\]' | \
  grep -F -v '/\*\*' | \
  $gsed -z 's/,\n */, /g' | \
  $gsed -z 's/:\n */: /g' | \
  $gsed -z 's/\n *->/ ->/g' | \
  grep -v '^\}$' | \
  $gsed 's/^ *pub/pub/' | \
  $gsed -z 's/\;\n/\n/g' | \
  grep 'pub fn' | \
  grep Handle | \
  grep -v roxyHandler | \
  grep -v '\bIdVector\b' | # name clash between rust::IdVector and JS::IdVector \
  grep -v 'pub fn Unbox' | # this function seems to be platform specific \
  grep -v 'CopyAsyncStack' | # arch-specific bindgen output
  $gsed 's/root::/raw::/g' |
  $gsed 's/Handle<\*mut JSObject>/HandleObject/g' |
  grep -F -v '> HandleObject' | # We are only wrapping handles in args not in results
  grep -v 'MutableHandleObjectVector' # GetDebuggeeGlobals has it
}

# usage find_latest_version_of_file_and_parse $input_file $out_wrapper_module_name
find_latest_version_of_file_and_parse() {
  # clone file and reformat (this is needed for grep_heur to work properly)
  cp jsapi-rs/src/$1/bindings.rs /tmp/wrap.rs
  rustfmt /tmp/wrap.rs --config max_width=1000
  
  printf "mod raw {
  #[allow(unused_imports)]
  pub use crate::raw::*;
  pub use crate::raw::JS::*;
  pub use crate::raw::JS::dbg::*;
  pub use crate::raw::JS::detail::*;
  pub use crate::raw::js::*;
  pub use crate::raw::jsglue::*;
}

" > "spidermonkey-rs/src/rust/jsapi_wrapped/$1_wrappers.rs"
  # parse reformated file
  grep_heur /tmp/wrap.rs | $gsed 's/\(.*\)/wrap!(raw: \1);/g' >> "spidermonkey-rs/src/rust/jsapi_wrapped/$1_wrappers.rs"
}

find_latest_version_of_file_and_parse jsapi
#find_latest_version_of_file_and_parse glue
