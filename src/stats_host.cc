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

#include "common_sandbox.h"
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

    if (baton->err.empty()) {
      // Stats Object
      Napi::Object info = Napi::Object::New(env);
      Napi::Array channels = Napi::Array::New(env);

      std::vector<ChannelStats>::iterator it;
      int i = 0;
      for (it = baton->channelStats.begin(); it < baton->channelStats.end(); it++, i++) {
        Napi::Object channelStat = Napi::Object::New(env);
        channelStat.Set("min", it->min);
        channelStat.Set("max", it->max);
        channelStat.Set("sum", it->sum);
        channelStat.Set("squaresSum", it->squaresSum);
        channelStat.Set("mean", it->mean);
        channelStat.Set("stdev", it->stdev);
        channelStat.Set("minX", it->minX);
        channelStat.Set("minY", it->minY);
        channelStat.Set("maxX", it->maxX);
        channelStat.Set("maxY", it->maxY);
        channels.Set(i, channelStat);
      }

      info.Set("channels", channels);
      info.Set("isOpaque", baton->isOpaque);
      info.Set("entropy", baton->entropy);
      info.Set("sharpness", baton->sharpness);
      Napi::Object dominant = Napi::Object::New(env);
      dominant.Set("r", baton->dominantRed);
      dominant.Set("g", baton->dominantGreen);
      dominant.Set("b", baton->dominantBlue);
      info.Set("dominant", dominant);
      Callback().MakeCallback(Receiver().Value(), { env.Null(), info });
    } else {
      Callback().MakeCallback(Receiver().Value(), { Napi::Error::New(env, baton->err).Value() });
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
  // V8 objects are converted to non-V8 types held in the baton struct
  StatsBaton *baton = CreateStatsBaton();
  Napi::Object options = info[0].As<Napi::Object>();

  // Input
  baton->input = sharp::CreateInputDescriptor(options.Get("input").As<Napi::Object>());

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
