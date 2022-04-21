#pragma once

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

using namespace rlbox;

rlbox_sandbox_vips* GetVipsSandbox();