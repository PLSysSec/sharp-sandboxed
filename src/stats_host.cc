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

class StatsWorker : public Napi::AsyncWorker {
 public:
  StatsWorker(Napi::Function callback, StatsBaton *baton, Napi::Function debuglog) :
    Napi::AsyncWorker(callback), baton(baton), debuglog(Napi::Persistent(debuglog)) {}
  ~StatsWorker() {}

  void Execute() {
    // Decrement queued task counter
    g_atomic_int_dec_and_test(&sharp::counterQueue);

    StatsWorkerExecute(baton);
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

    if (StatsBaton_GetErr(baton) == std::string("")) {
      // Stats Object
      Napi::Object info = Napi::Object::New(env);
      Napi::Array channels = Napi::Array::New(env);

      size_t baton_channelStats_size = StatsBaton_GetChannelStats_Size(baton);
      ChannelStats* baton_channelStats = StatsBaton_GetChannelStats(baton);

      for (size_t i = 0; i < baton_channelStats_size; i++) {
        ChannelStats& c = baton_channelStats[i];
        Napi::Object channelStat = Napi::Object::New(env);
        channelStat.Set("min", c.min);
        channelStat.Set("max", c.max);
        channelStat.Set("sum", c.sum);
        channelStat.Set("squaresSum", c.squaresSum);
        channelStat.Set("mean", c.mean);
        channelStat.Set("stdev", c.stdev);
        channelStat.Set("minX", c.minX);
        channelStat.Set("minY", c.minY);
        channelStat.Set("maxX", c.maxX);
        channelStat.Set("maxY", c.maxY);
        channels.Set(i, channelStat);
      }

      info.Set("channels", channels);
      info.Set("isOpaque", StatsBaton_GetIsOpaque(baton));
      info.Set("entropy", StatsBaton_GetEntropy(baton));
      info.Set("sharpness", StatsBaton_GetSharpness(baton));
      Napi::Object dominant = Napi::Object::New(env);
      dominant.Set("r", StatsBaton_GetDominantRed(baton));
      dominant.Set("g", StatsBaton_GetDominantGreen(baton));
      dominant.Set("b", StatsBaton_GetDominantBlue(baton));
      info.Set("dominant", dominant);
      Callback().MakeCallback(Receiver().Value(), { env.Null(), info });
    } else {
      std::string errString = StatsBaton_GetErr(baton);
      Callback().MakeCallback(Receiver().Value(), { Napi::Error::New(env, errString).Value() });
    }

    DestroyStatsBaton(baton);
  }

 private:
  StatsBaton* baton;
  Napi::FunctionReference debuglog;
};

/*
  stats(options, callback)
*/
Napi::Value stats(const Napi::CallbackInfo& info) {
  rlbox_sandbox_vips* sandbox = GetVipsSandbox();

  // V8 objects are converted to non-V8 types held in the baton struct
  StatsBaton *baton = CreateStatsBaton();
  Napi::Object options = info[0].As<Napi::Object>();

  // Input
  StatsBaton_SetInput(baton, sharp::CreateInputDescriptor(sandbox, options.Get("input").As<Napi::Object>()).UNSAFE_unverified());

  // Function to notify of libvips warnings
  Napi::Function debuglog = options.Get("debuglog").As<Napi::Function>();

  // Join queue for worker thread
  Napi::Function callback = info[1].As<Napi::Function>();
  StatsWorker *worker = new StatsWorker(callback, baton, debuglog);
  worker->Receiver().Set("options", options);
  worker->Queue();

  // Increment queued task counter
  g_atomic_int_inc(&sharp::counterQueue);

  return info.Env().Undefined();
}
