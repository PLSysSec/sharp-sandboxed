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

#include <cstdlib>
#include <string>
#include <string.h>
#include <vector>
#include <queue>
#include <map>
#include <mutex>  // NOLINT(build/c++11)

#include <napi.h>
#include <vips/vips8>

#include "common_sandbox.h"
#include "common_host.h"

using vips::VImage;

namespace sharp {

  // Convenience methods to access the attributes of a Napi::Object
  bool HasAttr(Napi::Object obj, std::string attr) {
    return obj.Has(attr);
  }
  std::string AttrAsStr(Napi::Object obj, std::string attr) {
    return obj.Get(attr).As<Napi::String>();
  }
  std::string AttrAsStr(Napi::Object obj, unsigned int const attr) {
    return obj.Get(attr).As<Napi::String>();
  }
  uint32_t AttrAsUint32(Napi::Object obj, std::string attr) {
    return obj.Get(attr).As<Napi::Number>().Uint32Value();
  }
  int32_t AttrAsInt32(Napi::Object obj, std::string attr) {
    return obj.Get(attr).As<Napi::Number>().Int32Value();
  }
  int32_t AttrAsInt32(Napi::Object obj, unsigned int const attr) {
    return obj.Get(attr).As<Napi::Number>().Int32Value();
  }
  double AttrAsDouble(Napi::Object obj, std::string attr) {
    return obj.Get(attr).As<Napi::Number>().DoubleValue();
  }
  double AttrAsDouble(Napi::Object obj, unsigned int const attr) {
    return obj.Get(attr).As<Napi::Number>().DoubleValue();
  }
  bool AttrAsBool(Napi::Object obj, std::string attr) {
    return obj.Get(attr).As<Napi::Boolean>().Value();
  }
  std::vector<double> AttrAsVectorOfDouble(Napi::Object obj, std::string attr) {
    Napi::Array napiArray = obj.Get(attr).As<Napi::Array>();
    std::vector<double> vectorOfDouble(napiArray.Length());
    for (unsigned int i = 0; i < napiArray.Length(); i++) {
      vectorOfDouble[i] = AttrAsDouble(napiArray, i);
    }
    return vectorOfDouble;
  }
  std::vector<int32_t> AttrAsInt32Vector(Napi::Object obj, std::string attr) {
    Napi::Array array = obj.Get(attr).As<Napi::Array>();
    std::vector<int32_t> vector(array.Length());
    for (unsigned int i = 0; i < array.Length(); i++) {
      vector[i] = AttrAsInt32(array, i);
    }
    return vector;
  }

  // Create an InputDescriptor instance from a Napi::Object describing an input image
  tainted_vips<InputDescriptor*> CreateInputDescriptor(rlbox_sandbox_vips* sandbox, Napi::Object input) {
    tainted_vips<InputDescriptor*> t_descriptor = sandbox->invoke_sandbox_function(CreateEmptyInputDescriptor);
    InputDescriptor* descriptor = t_descriptor.UNSAFE_unverified();
    if (HasAttr(input, "file")) {
      InputDescriptor_SetFile(descriptor, AttrAsStr(input, "file").c_str());
    } else if (HasAttr(input, "buffer")) {
      Napi::Buffer<char> buffer = input.Get("buffer").As<Napi::Buffer<char>>();
      InputDescriptor_SetBufferLength(descriptor, buffer.Length());
      InputDescriptor_SetBuffer(descriptor, buffer.Data());
      InputDescriptor_SetIsBuffer(descriptor, TRUE);
    }
    InputDescriptor_SetFailOnError(descriptor, AttrAsBool(input, "failOnError"));
    // Density for vector-based input
    if (HasAttr(input, "density")) {
      InputDescriptor_SetDensity(descriptor, AttrAsDouble(input, "density"));
    }
    // Raw pixel input
    if (HasAttr(input, "rawChannels")) {
      InputDescriptor_SetRawDepth(descriptor,
        vips_enum_from_nick(nullptr, VIPS_TYPE_BAND_FORMAT,
        AttrAsStr(input, "rawDepth").data()));
      InputDescriptor_SetRawChannels(descriptor, AttrAsUint32(input, "rawChannels"));
      InputDescriptor_SetRawWidth(descriptor, AttrAsUint32(input, "rawWidth"));
      InputDescriptor_SetRawHeight(descriptor, AttrAsUint32(input, "rawHeight"));
      InputDescriptor_SetRawPremultiplied(descriptor, AttrAsBool(input, "rawPremultiplied"));
    }
    // Multi-page input (GIF, TIFF, PDF)
    if (HasAttr(input, "pages")) {
      InputDescriptor_SetPages(descriptor, AttrAsInt32(input, "pages"));
    }
    if (HasAttr(input, "page")) {
      InputDescriptor_SetPage(descriptor, AttrAsUint32(input, "page"));
    }
    // Multi-level input (OpenSlide)
    if (HasAttr(input, "level")) {
      InputDescriptor_SetLevel(descriptor, AttrAsUint32(input, "level"));
    }
    // subIFD (OME-TIFF)
    if (HasAttr(input, "subifd")) {
      InputDescriptor_SetSubifd(descriptor, AttrAsInt32(input, "subifd"));
    }
    // Create new image
    if (HasAttr(input, "createChannels")) {
      InputDescriptor_SetCreateChannels(descriptor, AttrAsUint32(input, "createChannels"));
      InputDescriptor_SetCreateWidth(descriptor, AttrAsUint32(input, "createWidth"));
      InputDescriptor_SetCreateHeight(descriptor, AttrAsUint32(input, "createHeight"));
      if (HasAttr(input, "createNoiseType")) {
        InputDescriptor_SetCreateNoiseType(descriptor, AttrAsStr(input, "createNoiseType").c_str());
        InputDescriptor_SetCreateNoiseMean(descriptor, AttrAsDouble(input, "createNoiseMean"));
        InputDescriptor_SetCreateNoiseSigma(descriptor, AttrAsDouble(input, "createNoiseSigma"));
      } else {
        std::vector<double> createBackgroundVal = AttrAsVectorOfDouble(input, "createBackground");
        InputDescriptor_SetCreateBackground(descriptor, createBackgroundVal.data(), createBackgroundVal.size());
      }
    }
    // Limit input images to a given number of pixels, where pixels = width * height
    InputDescriptor_SetLimitInputPixels(descriptor, AttrAsUint32(input, "limitInputPixels"));
    // Allow switch from random to sequential access
    InputDescriptor_SetAccess(descriptor, (int) AttrAsBool(input, "sequentialRead") ? VIPS_ACCESS_SEQUENTIAL : VIPS_ACCESS_RANDOM);
    // Remove safety features and allow unlimited SVG/PNG input
    InputDescriptor_SetUnlimited(descriptor, AttrAsBool(input, "unlimited"));
    return t_descriptor;
  }

  // How many tasks are in the queue?
  volatile int counterQueue = 0;

  // How many tasks are being processed?
  volatile int counterProcess = 0;

  /*
    Called when a Buffer undergoes GC, required to support mixed runtime libraries in Windows
  */
  std::function<void(void*, char*)> FreeCallback = [](void*, char* data) {
    g_free(data);
  };

  std::function<void(void*, char*)> DeleteCallback = [](void*, char* data) {
    delete data;
  };

  /*
    Temporary buffer of warnings
  */
  std::queue<std::string> vipsWarnings;
  std::mutex vipsWarningsMutex;

  /*
    Called with warnings from the glib-registered "VIPS" domain
  */
  void VipsWarningCallback(char const* log_domain, GLogLevelFlags log_level, char const* message, void* ignore) {
    std::lock_guard<std::mutex> lock(vipsWarningsMutex);
    vipsWarnings.emplace(message);
  }

  /*
    Pop the oldest warning message from the queue
  */
  std::string VipsWarningPop() {
    std::string warning;
    std::lock_guard<std::mutex> lock(vipsWarningsMutex);
    if (!vipsWarnings.empty()) {
      warning = vipsWarnings.front();
      vipsWarnings.pop();
    }
    return warning;
  }

  tainted_vips<const char*> CopyStringToSandbox(rlbox_sandbox_vips* sandbox, const char* str) {
    if (!str) {
      return nullptr;
    }

    const uint32_t lenString = strnlen(str, std::numeric_limits<uint32_t>::max() - 1);
    const uint32_t len = lenString + 1;
    tainted_vips<char*> t_str = sandbox->malloc_in_sandbox<char>(len);
    strncpy(t_str.unverified_safe_pointer_because(len, "String copy"), str, len);
    return rlbox::sandbox_const_cast<const char*>(t_str);
  }

  tainted_vips<int> SandboxVipsEnumFromNick(rlbox_sandbox_vips* sandbox, const char *domain, GType type, const char *str) {
    tainted_vips<const char*> t_domain = CopyStringToSandbox(sandbox, domain);
    tainted_vips<const char*> t_str = CopyStringToSandbox(sandbox, str);

    tainted_vips<int> ret = sandbox->invoke_sandbox_function(vips_enum_from_nick, t_domain, type, t_str);

    if (t_domain) {
      sandbox->free_in_sandbox(t_domain);
    }
    if (t_str) {
      sandbox->free_in_sandbox(t_str);
    }
    return ret;
  }

  std::string SandboxVipsEnumNick(rlbox_sandbox_vips* sandbox, GType enm, tainted_vips<int> value) {
    tainted_vips<const char*> t_ret = sandbox->invoke_sandbox_function(vips_enum_nick, enm, value);
    std::string ret = t_ret.copy_and_verify_string([](std::string val) {
      return val;
    });
    return ret;
  }

}