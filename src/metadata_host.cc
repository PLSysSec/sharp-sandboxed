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

#include <napi.h>
#include <vips/vips8>

#include "common_sandbox.h"
#include "common_host.h"
#include "metadata_sandbox.h"
#include "metadata_host.h"

class MetadataWorker : public Napi::AsyncWorker {
 public:
  MetadataWorker(Napi::Function callback, MetadataBaton *baton, Napi::Function debuglog) :
    Napi::AsyncWorker(callback), baton(baton), debuglog(Napi::Persistent(debuglog)) {}
  ~MetadataWorker() {}

  void Execute() {
    MetadataWorkerExecute(baton);
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
      Napi::Object info = Napi::Object::New(env);
      info.Set("format", baton->format);
      if (baton->input->bufferLength > 0) {
        info.Set("size", baton->input->bufferLength);
      }
      info.Set("width", baton->width);
      info.Set("height", baton->height);
      info.Set("space", baton->space);
      info.Set("channels", baton->channels);
      info.Set("depth", baton->depth);
      if (baton->density > 0) {
        info.Set("density", baton->density);
      }
      if (!baton->chromaSubsampling.empty()) {
        info.Set("chromaSubsampling", baton->chromaSubsampling);
      }
      info.Set("isProgressive", baton->isProgressive);
      if (baton->paletteBitDepth > 0) {
        info.Set("paletteBitDepth", baton->paletteBitDepth);
      }
      if (baton->pages > 0) {
        info.Set("pages", baton->pages);
      }
      if (baton->pageHeight > 0) {
        info.Set("pageHeight", baton->pageHeight);
      }
      if (baton->loop >= 0) {
        info.Set("loop", baton->loop);
      }
      if (!baton->delay.empty()) {
        int i = 0;
        Napi::Array delay = Napi::Array::New(env, static_cast<size_t>(baton->delay.size()));
        for (int const d : baton->delay) {
          delay.Set(i++, d);
        }
        info.Set("delay", delay);
      }
      if (baton->pagePrimary > -1) {
        info.Set("pagePrimary", baton->pagePrimary);
      }
      if (!baton->compression.empty()) {
        info.Set("compression", baton->compression);
      }
      if (!baton->resolutionUnit.empty()) {
        info.Set("resolutionUnit", baton->resolutionUnit == "in" ? "inch" : baton->resolutionUnit);
      }
      if (!baton->levels.empty()) {
        int i = 0;
        Napi::Array levels = Napi::Array::New(env, static_cast<size_t>(baton->levels.size()));
        for (std::pair<int, int> const &l : baton->levels) {
          Napi::Object level = Napi::Object::New(env);
          level.Set("width", l.first);
          level.Set("height", l.second);
          levels.Set(i++, level);
        }
        info.Set("levels", levels);
      }
      if (baton->subifds > 0) {
        info.Set("subifds", baton->subifds);
      }
      if (!baton->background.empty()) {
        if (baton->background.size() == 3) {
          Napi::Object background = Napi::Object::New(env);
          background.Set("r", baton->background[0]);
          background.Set("g", baton->background[1]);
          background.Set("b", baton->background[2]);
          info.Set("background", background);
        } else {
          info.Set("background", baton->background[0]);
        }
      }
      info.Set("hasProfile", baton->hasProfile);
      info.Set("hasAlpha", baton->hasAlpha);
      if (baton->orientation > 0) {
        info.Set("orientation", baton->orientation);
      }
      if (baton->exifLength > 0) {
        info.Set("exif", Napi::Buffer<char>::New(env, baton->exif, baton->exifLength, sharp::FreeCallback));
      }
      if (baton->iccLength > 0) {
        info.Set("icc", Napi::Buffer<char>::New(env, baton->icc, baton->iccLength, sharp::FreeCallback));
      }
      if (baton->iptcLength > 0) {
        info.Set("iptc", Napi::Buffer<char>::New(env, baton->iptc, baton->iptcLength, sharp::FreeCallback));
      }
      if (baton->xmpLength > 0) {
        info.Set("xmp", Napi::Buffer<char>::New(env, baton->xmp, baton->xmpLength, sharp::FreeCallback));
      }
      if (baton->tifftagPhotoshopLength > 0) {
        info.Set("tifftagPhotoshop",
          Napi::Buffer<char>::New(env, baton->tifftagPhotoshop, baton->tifftagPhotoshopLength, sharp::FreeCallback));
      }
      Callback().MakeCallback(Receiver().Value(), { env.Null(), info });
    } else {
      Callback().MakeCallback(Receiver().Value(), { Napi::Error::New(env, baton->err).Value() });
    }

    delete baton->input;
    delete baton;
  }

 private:
  MetadataBaton* baton;
  Napi::FunctionReference debuglog;
};

/*
  metadata(options, callback)
*/
Napi::Value metadata(const Napi::CallbackInfo& info) {
  // V8 objects are converted to non-V8 types held in the baton struct
  MetadataBaton *baton = new MetadataBaton;
  Napi::Object options = info[0].As<Napi::Object>();

  // Input
  baton->input = sharp::CreateInputDescriptor(options.Get("input").As<Napi::Object>());

  // Function to notify of libvips warnings
  Napi::Function debuglog = options.Get("debuglog").As<Napi::Function>();

  // Join queue for worker thread
  Napi::Function callback = info[1].As<Napi::Function>();
  MetadataWorker *worker = new MetadataWorker(callback, baton, debuglog);
  worker->Receiver().Set("options", options);
  worker->Queue();

  // Increment queued task counter
  g_atomic_int_inc(&sharp::counterQueue);

  return info.Env().Undefined();
}
