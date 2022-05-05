// Copyright 2013, 2014, 2015, 2016, 2017, 2018, 2019, 2020 Lovell Fuller and contributors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <numeric>
#include <vector>
#include <iostream>

#include <napi.h>
#include <vips/vips8>

#include "common_host.h"
#include "stats_sandbox.h"
#include "stats_host.h"


#include "rlbox_mgr.h"
rlbox_load_structs_from_library(vips);

class StatsWorker : public Napi::AsyncWorker {
 public:
  StatsWorker(Napi::Function callback, tainted_vips<StatsBaton*> t_baton, Napi::Function debuglog, rlbox_sandbox_vips* sandbox) :
    Napi::AsyncWorker(callback), t_baton(t_baton), debuglog(Napi::Persistent(debuglog)), sandbox(sandbox) {}
  ~StatsWorker() {}

  void Execute() {
    // Decrement queued task counter
    g_atomic_int_dec_and_test(&sharp::counterQueue);

    sandbox->invoke_sandbox_function(StatsWorkerExecute, t_baton);
  }

  void OnOK() {
    Napi::Env env = Env();
    Napi::HandleScope scope(env);

    // Handle warnings
    std::string warning = sharp::VipsWarningPop();
    while (!warning.empty()) {
      debuglog.Call({ Napi::String::New(env, warning) });
      warning = sharp::VipsWarningPop();
    }

    std::string errString = sandbox->invoke_sandbox_function(StatsBaton_GetErr, t_baton).copy_and_verify_string([](std::string val) {
      // Worst case, the library says there is an error when there isn't
      return val;
    });

    const char image_attrib_reason[] = "Reading attributes of the image for the first and only time.";


    if (errString == std::string("")) {
      // Stats Object
      Napi::Object info = Napi::Object::New(env);
      Napi::Array channels = Napi::Array::New(env);

      size_t baton_channelStats_size = sandbox->invoke_sandbox_function(StatsBaton_GetChannelStats_Size, t_baton).unverified_safe_because(image_attrib_reason);
      tainted_vips<ChannelStats*> baton_channelStats = sandbox->invoke_sandbox_function(StatsBaton_GetChannelStats, t_baton);

      for (size_t i = 0; i < baton_channelStats_size; i++) {
        Napi::Object channelStat = Napi::Object::New(env);
        channelStat.Set("min", baton_channelStats[i].min.unverified_safe_because(image_attrib_reason));
        channelStat.Set("max", baton_channelStats[i].max.unverified_safe_because(image_attrib_reason));
        channelStat.Set("sum", baton_channelStats[i].sum.unverified_safe_because(image_attrib_reason));
        channelStat.Set("squaresSum", baton_channelStats[i].squaresSum.unverified_safe_because(image_attrib_reason));
        channelStat.Set("mean", baton_channelStats[i].mean.unverified_safe_because(image_attrib_reason));
        channelStat.Set("stdev", baton_channelStats[i].stdev.unverified_safe_because(image_attrib_reason));
        channelStat.Set("minX", baton_channelStats[i].minX.unverified_safe_because(image_attrib_reason));
        channelStat.Set("minY", baton_channelStats[i].minY.unverified_safe_because(image_attrib_reason));
        channelStat.Set("maxX", baton_channelStats[i].maxX.unverified_safe_because(image_attrib_reason));
        channelStat.Set("maxY", baton_channelStats[i].maxY.unverified_safe_because(image_attrib_reason));
        channels.Set(i, channelStat);
      }

      info.Set("channels", channels);
      info.Set("isOpaque", sandbox->invoke_sandbox_function(StatsBaton_GetIsOpaque, t_baton).unverified_safe_because(image_attrib_reason));
      info.Set("entropy", sandbox->invoke_sandbox_function(StatsBaton_GetEntropy, t_baton).unverified_safe_because(image_attrib_reason));
      info.Set("sharpness", sandbox->invoke_sandbox_function(StatsBaton_GetSharpness, t_baton).unverified_safe_because(image_attrib_reason));
      Napi::Object dominant = Napi::Object::New(env);
      dominant.Set("r", sandbox->invoke_sandbox_function(StatsBaton_GetDominantRed, t_baton).unverified_safe_because(image_attrib_reason));
      dominant.Set("g", sandbox->invoke_sandbox_function(StatsBaton_GetDominantGreen, t_baton).unverified_safe_because(image_attrib_reason));
      dominant.Set("b", sandbox->invoke_sandbox_function(StatsBaton_GetDominantBlue, t_baton).unverified_safe_because(image_attrib_reason));
      info.Set("dominant", dominant);
      Callback().MakeCallback(Receiver().Value(), { env.Null(), info });
    } else {
      auto errString = sandbox->invoke_sandbox_function(StatsBaton_GetErr, t_baton)
      .copy_and_verify_string([](std::string val) {
        // Worst case you'd get a bad error message
        return val;
      });
      Callback().MakeCallback(Receiver().Value(), { Napi::Error::New(env, errString).Value() });
    }

    sandbox->invoke_sandbox_function(DestroyStatsBaton, t_baton);
  }

 private:
  tainted_vips<StatsBaton*> t_baton;
  Napi::FunctionReference debuglog;
  rlbox_sandbox_vips* sandbox;
};

/*
  stats(options, callback)
*/
Napi::Value stats(const Napi::CallbackInfo& info) {
  rlbox_sandbox_vips* sandbox = GetVipsSandbox();

  // V8 objects are converted to non-V8 types held in the baton struct
  tainted_vips<StatsBaton*> t_baton = sandbox->invoke_sandbox_function(CreateStatsBaton);
  Napi::Object options = info[0].As<Napi::Object>();

  // Input
  sandbox->invoke_sandbox_function(StatsBaton_SetInput, t_baton, sharp::CreateInputDescriptor(sandbox, options.Get("input").As<Napi::Object>()));

  // Function to notify of libvips warnings
  Napi::Function debuglog = options.Get("debuglog").As<Napi::Function>();

  // Join queue for worker thread
  Napi::Function callback = info[1].As<Napi::Function>();
  StatsWorker *worker = new StatsWorker(callback, t_baton, debuglog, sandbox);
  worker->Receiver().Set("options", options);
  worker->Queue();

  // Increment queued task counter
  g_atomic_int_inc(&sharp::counterQueue);

  return info.Env().Undefined();
}
