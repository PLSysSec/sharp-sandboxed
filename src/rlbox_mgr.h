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

#include "stats_sandbox.h"
#include "metadata_sandbox.h"

#define sandbox_fields_reflection_vips_class_ChannelStats(f, g, ...) \
  f(int,    min,       FIELD_NORMAL, ##__VA_ARGS__) g()                          \
  f(int,    max,       FIELD_NORMAL, ##__VA_ARGS__) g()                          \
  f(double, sum,       FIELD_NORMAL, ##__VA_ARGS__) g()                          \
  f(double, squaresSum,FIELD_NORMAL, ##__VA_ARGS__) g()                          \
  f(double, mean,      FIELD_NORMAL, ##__VA_ARGS__) g()                          \
  f(double, stdev,     FIELD_NORMAL, ##__VA_ARGS__) g()                          \
  f(int,    minX,      FIELD_NORMAL, ##__VA_ARGS__) g()                          \
  f(int,    minY,      FIELD_NORMAL, ##__VA_ARGS__) g()                          \
  f(int,    maxX,      FIELD_NORMAL, ##__VA_ARGS__) g()                          \
  f(int,    maxY,      FIELD_NORMAL, ##__VA_ARGS__) g()

#define sandbox_fields_reflection_vips_class_MetadataDimension(f, g, ...) \
  f(int, width, FIELD_NORMAL, ##__VA_ARGS__) g()                          \
  f(int, height, FIELD_NORMAL, ##__VA_ARGS__) g()

#define sandbox_fields_reflection_vips_allClasses(f, ...)                 \
  f(MetadataDimension, vips, ##__VA_ARGS__) \
  f(ChannelStats, vips, ##__VA_ARGS__)

rlbox_sandbox_vips* GetVipsSandbox();