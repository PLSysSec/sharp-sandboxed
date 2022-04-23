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

#include "rlbox_mgr.h"
rlbox_load_structs_from_library(vips);

class MetadataWorker : public Napi::AsyncWorker {
 public:
  MetadataWorker(Napi::Function callback, tainted_vips<MetadataBaton*> t_baton, Napi::Function debuglog, rlbox_sandbox_vips* sandbox) :
    Napi::AsyncWorker(callback), t_baton(t_baton), debuglog(Napi::Persistent(debuglog)), sandbox(sandbox) {}
  ~MetadataWorker() {}

  void Execute() {
    // Decrement queued task counter
    g_atomic_int_dec_and_test(&sharp::counterQueue);

    sandbox->invoke_sandbox_function(MetadataWorkerExecute, t_baton);
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

    std::string errString = sandbox->invoke_sandbox_function(MetadataBaton_GetErr, t_baton).copy_and_verify_string([](std::string val) {
      // Worst case, the library says there is an error when there isn't
      return val;
    });

    const char image_attrib_reason[] = "Reading attributes of the image for the first and only time.";

    if (errString == std::string("")) {
      Napi::Object info = Napi::Object::New(env);
      auto formatString = sandbox->invoke_sandbox_function(MetadataBaton_GetFormat, t_baton)
        .copy_and_verify_string([](std::string val) {
          // UNSAFE --- sanity check?
          return val;
        });
      info.Set("format", formatString.c_str());
      tainted_vips<InputDescriptor*> input = sandbox->invoke_sandbox_function(MetadataBaton_GetInput, t_baton);
      size_t bufferLength = sandbox->invoke_sandbox_function(InputDescriptor_GetBufferLength, input).unverified_safe_because(image_attrib_reason);

      if (bufferLength > 0) {
        info.Set("size", bufferLength);
      }
      info.Set("width", sandbox->invoke_sandbox_function(MetadataBaton_GetWidth, t_baton).unverified_safe_because(image_attrib_reason));
      info.Set("height", sandbox->invoke_sandbox_function(MetadataBaton_GetHeight, t_baton).unverified_safe_because(image_attrib_reason));
      auto spaceString = sandbox->invoke_sandbox_function(MetadataBaton_GetSpace, t_baton)
        .copy_and_verify_string([](std::string val) {
          // UNSAFE --- sanity check?
          return val;
        });
      info.Set("space", spaceString.c_str());
      info.Set("channels", sandbox->invoke_sandbox_function(MetadataBaton_GetChannels, t_baton).unverified_safe_because(image_attrib_reason));
      auto depthString = sandbox->invoke_sandbox_function(MetadataBaton_GetDepth, t_baton)
        .copy_and_verify_string([](std::string val) {
          // UNSAFE --- sanity check?
          return val;
        });
      info.Set("depth", depthString.c_str());
      int density = sandbox->invoke_sandbox_function(MetadataBaton_GetDensity, t_baton).unverified_safe_because(image_attrib_reason);
      if (density > 0) {
        info.Set("density", density);
      }
      auto chromaString = sandbox->invoke_sandbox_function(MetadataBaton_GetChromaSubsampling, t_baton)
        .copy_and_verify_string([](std::string val) {
          // UNSAFE --- sanity check?
          return val;
        });
      if (chromaString != std::string("")) {
        info.Set("chromaSubsampling", chromaString);
      }
      info.Set("isProgressive", sandbox->invoke_sandbox_function(MetadataBaton_GetIsProgressive, t_baton).unverified_safe_because(image_attrib_reason));
      int paletteBitDepth = sandbox->invoke_sandbox_function(MetadataBaton_GetPaletteBitDepth, t_baton).unverified_safe_because(image_attrib_reason);
      if (paletteBitDepth > 0) {
        info.Set("paletteBitDepth", paletteBitDepth);
      }
      int pages = sandbox->invoke_sandbox_function(MetadataBaton_GetPages, t_baton).unverified_safe_because(image_attrib_reason);
      if (pages > 0) {
        info.Set("pages", pages);
      }
      int pageHeight = sandbox->invoke_sandbox_function(MetadataBaton_GetPageHeight, t_baton).unverified_safe_because(image_attrib_reason);
      if (pageHeight > 0) {
        info.Set("pageHeight", pageHeight);
      }
      int loop = sandbox->invoke_sandbox_function(MetadataBaton_GetLoop, t_baton).unverified_safe_because(image_attrib_reason);
      if (loop >= 0) {
        info.Set("loop", loop);
      }
      size_t baton_delay_size = sandbox->invoke_sandbox_function(MetadataBaton_GetDelay_Size, t_baton).unverified_safe_because(image_attrib_reason);
      if (baton_delay_size != 0) {
        tainted_vips<int*> baton_delay = sandbox->invoke_sandbox_function(MetadataBaton_GetDelay, t_baton);
        Napi::Array delay = Napi::Array::New(env, static_cast<size_t>(baton_delay_size));
        for (size_t i = 0; i < baton_delay_size; i++) {
          int const d = baton_delay[i].unverified_safe_because(image_attrib_reason);
          delay.Set(i, d);
        }
        info.Set("delay", delay);
      }
      int pagePrimary = sandbox->invoke_sandbox_function(MetadataBaton_GetPagePrimary, t_baton).unverified_safe_because(image_attrib_reason);
      if (pagePrimary > -1) {
        info.Set("pagePrimary", pagePrimary);
      }
      auto compressionString = sandbox->invoke_sandbox_function(MetadataBaton_GetCompression, t_baton)
        .copy_and_verify_string([](std::string val) {
          // UNSAFE --- sanity check?
          return val;
        });
      if (compressionString != std::string("")) {
        info.Set("compression", compressionString);
      }
      auto resolutionUnitString = sandbox->invoke_sandbox_function(MetadataBaton_GetResolutionUnit, t_baton)
        .copy_and_verify_string([](std::string val) {
          // UNSAFE --- sanity check?
          return val;
        });
      if (resolutionUnitString != std::string("")) {
        info.Set("resolutionUnit", resolutionUnitString == std::string("in") ? std::string("inch") : resolutionUnitString);
      }
      size_t baton_levels_size = sandbox->invoke_sandbox_function(MetadataBaton_GetLevels_Size, t_baton).unverified_safe_because(image_attrib_reason);
      if (baton_levels_size != 0) {
        tainted_vips<MetadataDimension*> baton_levels = sandbox->invoke_sandbox_function(MetadataBaton_GetLevels, t_baton);
        Napi::Array levels = Napi::Array::New(env, static_cast<size_t>(baton_levels_size));
        for (size_t i = 0; i < baton_levels_size; i++) {
          Napi::Object level = Napi::Object::New(env);
          level.Set("width", baton_levels[i].width.unverified_safe_because(image_attrib_reason));
          level.Set("height", baton_levels[i].height.unverified_safe_because(image_attrib_reason));
          levels.Set(i, level);
        }
        info.Set("levels", levels);
      }
      int subifds = sandbox->invoke_sandbox_function(MetadataBaton_GetSubifds, t_baton).unverified_safe_because(image_attrib_reason);
      if (subifds > 0) {
        info.Set("subifds", subifds);
      }
      size_t backgroundSize = sandbox->invoke_sandbox_function(MetadataBaton_GetBackground_Size, t_baton).unverified_safe_because(image_attrib_reason);
      if (backgroundSize != 0) {
        tainted_vips<double*> baton_background = sandbox->invoke_sandbox_function(MetadataBaton_GetBackground, t_baton);
        if (backgroundSize == 3) {
          Napi::Object background = Napi::Object::New(env);
          background.Set("r", baton_background[0].unverified_safe_because(image_attrib_reason));
          background.Set("g", baton_background[1].unverified_safe_because(image_attrib_reason));
          background.Set("b", baton_background[2].unverified_safe_because(image_attrib_reason));
          info.Set("background", background);
        } else {
          info.Set("background", baton_background[0].unverified_safe_because(image_attrib_reason));
        }
      }
      info.Set("hasProfile", sandbox->invoke_sandbox_function(MetadataBaton_GetHasProfile, t_baton).unverified_safe_because(image_attrib_reason));
      info.Set("hasAlpha", sandbox->invoke_sandbox_function(MetadataBaton_GetHasAlpha, t_baton).unverified_safe_because(image_attrib_reason));
      int orientation = sandbox->invoke_sandbox_function(MetadataBaton_GetOrientation, t_baton).unverified_safe_because(image_attrib_reason);
      if (orientation > 0) {
        info.Set("orientation", orientation);
      }

      {
        uint32_t exifLength = static_cast<uint32_t>(sandbox->invoke_sandbox_function(MetadataBaton_GetExifLength, t_baton).unverified_safe_because(image_attrib_reason));
        if (exifLength > 0) {
          tainted_vips<char*> t_buffer_ref = rlbox::sandbox_static_cast<char*>(sandbox->invoke_sandbox_function(MetadataBaton_GetExif, t_baton));
          char* buffer_ref = t_buffer_ref.copy_and_verify_range(
            [](std::unique_ptr<char[]> val) {
            return val.release();
          }, exifLength);
          sandbox->free_in_sandbox(t_buffer_ref);

          Napi::Buffer<char> data = Napi::Buffer<char>::New(env, buffer_ref,
            exifLength, sharp::DeleteCallback);

          info.Set("exif", data);
        }
      }
      {
        uint32_t iccLength = static_cast<uint32_t>(sandbox->invoke_sandbox_function(MetadataBaton_GetIccLength, t_baton).unverified_safe_because(image_attrib_reason));
        if (iccLength > 0) {
          tainted_vips<char*> t_buffer_ref = rlbox::sandbox_static_cast<char*>(sandbox->invoke_sandbox_function(MetadataBaton_GetIcc, t_baton));
          char* buffer_ref = t_buffer_ref.copy_and_verify_range(
            [](std::unique_ptr<char[]> val) {
            return val.release();
          }, iccLength);
          sandbox->free_in_sandbox(t_buffer_ref);

          Napi::Buffer<char> data = Napi::Buffer<char>::New(env, buffer_ref,
            iccLength, sharp::DeleteCallback);

          info.Set("icc", data);
        }
      }
      {
        uint32_t iptcLength = static_cast<uint32_t>(sandbox->invoke_sandbox_function(MetadataBaton_GetIptcLength, t_baton).unverified_safe_because(image_attrib_reason));
        if (iptcLength > 0) {
          tainted_vips<char*> t_buffer_ref = rlbox::sandbox_static_cast<char*>(sandbox->invoke_sandbox_function(MetadataBaton_GetIptc, t_baton));
          char* buffer_ref = t_buffer_ref.copy_and_verify_range(
            [](std::unique_ptr<char[]> val) {
            return val.release();
          }, iptcLength);
          sandbox->free_in_sandbox(t_buffer_ref);

          Napi::Buffer<char> data = Napi::Buffer<char>::New(env, buffer_ref,
            iptcLength, sharp::DeleteCallback);

          info.Set("iptc", data);
        }
      }
      {
        uint32_t xmpLength = static_cast<uint32_t>(sandbox->invoke_sandbox_function(MetadataBaton_GetXmpLength, t_baton).unverified_safe_because(image_attrib_reason));
        if (xmpLength > 0) {
          tainted_vips<char*> t_buffer_ref = rlbox::sandbox_static_cast<char*>(sandbox->invoke_sandbox_function(MetadataBaton_GetXmp, t_baton));
          char* buffer_ref = t_buffer_ref.copy_and_verify_range(
            [](std::unique_ptr<char[]> val) {
            return val.release();
          }, xmpLength);
          sandbox->free_in_sandbox(t_buffer_ref);

          Napi::Buffer<char> data = Napi::Buffer<char>::New(env, buffer_ref,
            xmpLength, sharp::DeleteCallback);

          info.Set("xmp", data);
        }
      }
      {
        uint32_t tifftagPhotoshopLength = static_cast<uint32_t>(sandbox->invoke_sandbox_function(MetadataBaton_GetTifftagPhotoshopLength, t_baton).unverified_safe_because(image_attrib_reason));
        if (tifftagPhotoshopLength > 0) {
          tainted_vips<char*> t_buffer_ref = rlbox::sandbox_static_cast<char*>(sandbox->invoke_sandbox_function(MetadataBaton_GetTifftagPhotoshop, t_baton));
          char* buffer_ref = t_buffer_ref.copy_and_verify_range(
            [](std::unique_ptr<char[]> val) {
            return val.release();
          }, tifftagPhotoshopLength);
          sandbox->free_in_sandbox(t_buffer_ref);

          Napi::Buffer<char> data = Napi::Buffer<char>::New(env, buffer_ref,
            tifftagPhotoshopLength, sharp::DeleteCallback);

          info.Set("tifftagPhotoshop", data);
        }
      }
      Callback().MakeCallback(Receiver().Value(), { env.Null(), info });
    } else {
      auto errString = sandbox->invoke_sandbox_function(MetadataBaton_GetErr, t_baton)
      .copy_and_verify_string([](std::string val) {
        // Worst case you'd get a bad error message
        return val;
      });
      Callback().MakeCallback(Receiver().Value(), { Napi::Error::New(env, errString.c_str()).Value() });
    }

    sandbox->invoke_sandbox_function(DestroyMetadataBaton, t_baton);
  }

 private:
  tainted_vips<MetadataBaton*> t_baton;
  Napi::FunctionReference debuglog;
  rlbox_sandbox_vips* sandbox;
};

/*
  metadata(options, callback)
*/
Napi::Value metadata(const Napi::CallbackInfo& info) {
  rlbox_sandbox_vips* sandbox = GetVipsSandbox();

  // V8 objects are converted to non-V8 types held in the baton struct
  tainted_vips<MetadataBaton*> t_baton = sandbox->invoke_sandbox_function(CreateMetadataBaton);
  Napi::Object options = info[0].As<Napi::Object>();

  // Input
  tainted_vips<InputDescriptor*> inputdesc = sharp::CreateInputDescriptor(sandbox, options.Get("input").As<Napi::Object>());
  sandbox->invoke_sandbox_function(MetadataBaton_SetInput, t_baton, inputdesc);

  // Function to notify of libvips warnings
  Napi::Function debuglog = options.Get("debuglog").As<Napi::Function>();

  // Join queue for worker thread
  Napi::Function callback = info[1].As<Napi::Function>();
  MetadataWorker *worker = new MetadataWorker(callback, t_baton, debuglog, sandbox);
  worker->Receiver().Set("options", options);
  worker->Queue();

  // Increment queued task counter
  g_atomic_int_inc(&sharp::counterQueue);

  return info.Env().Undefined();
}
