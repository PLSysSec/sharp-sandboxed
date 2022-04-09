#pragma once

#include "rlbox_vips_structs.h"

#define RLBOX_SINGLE_THREADED_INVOCATIONS

#ifdef WASM_SANDBOXED_VIPS
#  error "Not implemented"
#else
#  define RLBOX_USE_STATIC_CALLS() rlbox_noop_sandbox_lookup_symbol
#  include "rlbox_noop_sandbox.hpp"
#endif

#include "rlbox.hpp"

#ifdef WASM_SANDBOXED_VIPS
#  error "Not implemented"
#else
  RLBOX_DEFINE_BASE_TYPES_FOR(vips, noop);
#endif

rlbox_load_structs_from_library(vips);
