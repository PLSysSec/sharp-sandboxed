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

#define sandbox_fields_reflection_vips_class_MetadataDimension(f, g, ...) \
  f(int, width, FIELD_NORMAL, ##__VA_ARGS__) g()                          \
  f(int, height, FIELD_NORMAL, ##__VA_ARGS__) g()

#define sandbox_fields_reflection_vips_allClasses(f, ...)                 \
  f(MetadataDimension, vips, ##__VA_ARGS__)

rlbox_sandbox_vips* GetVipsSandbox();