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

#include "common_host.h"
#include "common_sandbox.h"
#include "metadata_sandbox.h"
#include "metadata_host.h"

class MetadataWorker : public Napi::AsyncWorker {
 public:
  MetadataWorker(Napi::Function callback, MetadataBaton *baton, Napi::Function debuglog) :
    Napi::AsyncWorker(callback), baton(baton), debuglog(Napi::Persistent(debuglog)) {}
  ~MetadataWorker() {}

  void Execute() {
    // Decrement queued task counter
    g_atomic_int_dec_and_test(&sharp::counterQueue);

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

    if (MetadataBaton_GetErr(baton) == std::string("")) {
      Napi::Object info = Napi::Object::New(env);
      info.Set("format", MetadataBaton_GetFormat(baton));
      auto input = MetadataBaton_GetInput(baton);
      auto bufferLength = InputDescriptor_GetBufferLength(input);
      if (bufferLength > 0) {
        info.Set("size", bufferLength);
      }
      info.Set("width", MetadataBaton_GetWidth(baton));
      info.Set("height", MetadataBaton_GetHeight(baton));
      info.Set("space", MetadataBaton_GetSpace(baton));
      info.Set("channels", MetadataBaton_GetChannels(baton));
      info.Set("depth", MetadataBaton_GetDepth(baton));
      if (MetadataBaton_GetDensity(baton) > 0) {
        info.Set("density", MetadataBaton_GetDensity(baton));
      }
      if (MetadataBaton_GetChromaSubsampling(baton) != std::string("")) {
        info.Set("chromaSubsampling", MetadataBaton_GetChromaSubsampling(baton));
      }
      info.Set("isProgressive", MetadataBaton_GetIsProgressive(baton));
      if (MetadataBaton_GetPaletteBitDepth(baton) > 0) {
        info.Set("paletteBitDepth", MetadataBaton_GetPaletteBitDepth(baton));
      }
      if (MetadataBaton_GetPages(baton) > 0) {
        info.Set("pages", MetadataBaton_GetPages(baton));
      }
      if (MetadataBaton_GetPageHeight(baton) > 0) {
        info.Set("pageHeight", MetadataBaton_GetPageHeight(baton));
      }
      if (MetadataBaton_GetLoop(baton) >= 0) {
        info.Set("loop", MetadataBaton_GetLoop(baton));
      }
      if (!MetadataBaton_GetDelay_Empty(baton)) {
        size_t baton_delay_size = MetadataBaton_GetDelay_Size(baton);
        int* baton_delay = MetadataBaton_GetDelay(baton);
        Napi::Array delay = Napi::Array::New(env, static_cast<size_t>(baton_delay_size));
        for (size_t i = 0; i < baton_delay_size; i++) {
          int const d = baton_delay[i];
          delay.Set(i, d);
        }
        info.Set("delay", delay);
      }
      if (MetadataBaton_GetPagePrimary(baton) > -1) {
        info.Set("pagePrimary", MetadataBaton_GetPagePrimary(baton));
      }
      if (MetadataBaton_GetCompression(baton) != std::string("")) {
        info.Set("compression", MetadataBaton_GetCompression(baton));
      }
      if (MetadataBaton_GetResolutionUnit(baton) != std::string("")) {
        info.Set("resolutionUnit", MetadataBaton_GetResolutionUnit(baton) == std::string("in") ? std::string("inch") : MetadataBaton_GetResolutionUnit(baton));
      }
      if (!MetadataBaton_GetLevels_Empty(baton)) {
        size_t baton_levels_size = MetadataBaton_GetLevels_Size(baton);
        MetadataDimension* baton_levels = MetadataBaton_GetLevels(baton);
        Napi::Array levels = Napi::Array::New(env, static_cast<size_t>(baton_levels_size));
        for (size_t i = 0; i < baton_levels_size; i++) {
          MetadataDimension const &l = baton_levels[i];
          Napi::Object level = Napi::Object::New(env);
          level.Set("width", l.width);
          level.Set("height", l.height);
          levels.Set(i, level);
        }
        info.Set("levels", levels);
      }
      if (MetadataBaton_GetSubifds(baton) > 0) {
        info.Set("subifds", MetadataBaton_GetSubifds(baton));
      }
      if (!MetadataBaton_GetBackground_Empty(baton)) {
        auto baton_background = MetadataBaton_GetBackground(baton);
        if (MetadataBaton_GetBackground_Size(baton) == 3) {
          Napi::Object background = Napi::Object::New(env);
          background.Set("r", baton_background[0]);
          background.Set("g", baton_background[1]);
          background.Set("b", baton_background[2]);
          info.Set("background", background);
        } else {
          info.Set("background", baton_background[0]);
        }
      }
      info.Set("hasProfile", MetadataBaton_GetHasProfile(baton));
      info.Set("hasAlpha", MetadataBaton_GetHasAlpha(baton));
      if (MetadataBaton_GetOrientation(baton) > 0) {
        info.Set("orientation", MetadataBaton_GetOrientation(baton));
      }
      if (MetadataBaton_GetExifLength(baton) > 0) {
        info.Set("exif", Napi::Buffer<char>::New(env, MetadataBaton_GetExif(baton), MetadataBaton_GetExifLength(baton), sharp::FreeCallback));
      }
      if (MetadataBaton_GetIccLength(baton) > 0) {
        info.Set("icc", Napi::Buffer<char>::New(env, MetadataBaton_GetIcc(baton), MetadataBaton_GetIccLength(baton), sharp::FreeCallback));
      }
      if (MetadataBaton_GetIptcLength(baton) > 0) {
        info.Set("iptc", Napi::Buffer<char>::New(env, MetadataBaton_GetIptc(baton), MetadataBaton_GetIptcLength(baton), sharp::FreeCallback));
      }
      if (MetadataBaton_GetXmpLength(baton) > 0) {
        info.Set("xmp", Napi::Buffer<char>::New(env, MetadataBaton_GetXmp(baton), MetadataBaton_GetXmpLength(baton), sharp::FreeCallback));
      }
      if (MetadataBaton_GetTifftagPhotoshopLength(baton) > 0) {
        info.Set("tifftagPhotoshop",
          Napi::Buffer<char>::New(env, MetadataBaton_GetTifftagPhotoshop(baton), MetadataBaton_GetTifftagPhotoshopLength(baton), sharp::FreeCallback));
      }
      Callback().MakeCallback(Receiver().Value(), { env.Null(), info });
    } else {
      Callback().MakeCallback(Receiver().Value(), { Napi::Error::New(env, MetadataBaton_GetErr(baton)).Value() });
    }

    DestroyMetadataBaton(baton);
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
  MetadataBaton *baton = CreateMetadataBaton();
  Napi::Object options = info[0].As<Napi::Object>();

  // Input
  MetadataBaton_SetInput(baton, sharp::CreateInputDescriptor(options.Get("input").As<Napi::Object>()));

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
