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
  InputDescriptor* CreateInputDescriptor(Napi::Object input) {
    InputDescriptor *descriptor = new InputDescriptor;
    if (HasAttr(input, "file")) {
      descriptor->file = AttrAsStr(input, "file");
    } else if (HasAttr(input, "buffer")) {
      Napi::Buffer<char> buffer = input.Get("buffer").As<Napi::Buffer<char>>();
      descriptor->bufferLength = buffer.Length();
      descriptor->buffer = buffer.Data();
      descriptor->isBuffer = TRUE;
    }
    descriptor->failOnError = AttrAsBool(input, "failOnError");
    // Density for vector-based input
    if (HasAttr(input, "density")) {
      descriptor->density = AttrAsDouble(input, "density");
    }
    // Raw pixel input
    if (HasAttr(input, "rawChannels")) {
      descriptor->rawDepth = static_cast<VipsBandFormat>(
        vips_enum_from_nick(nullptr, VIPS_TYPE_BAND_FORMAT,
        AttrAsStr(input, "rawDepth").data()));
      descriptor->rawChannels = AttrAsUint32(input, "rawChannels");
      descriptor->rawWidth = AttrAsUint32(input, "rawWidth");
      descriptor->rawHeight = AttrAsUint32(input, "rawHeight");
      descriptor->rawPremultiplied = AttrAsBool(input, "rawPremultiplied");
    }
    // Multi-page input (GIF, TIFF, PDF)
    if (HasAttr(input, "pages")) {
      descriptor->pages = AttrAsInt32(input, "pages");
    }
    if (HasAttr(input, "page")) {
      descriptor->page = AttrAsUint32(input, "page");
    }
    // Multi-level input (OpenSlide)
    if (HasAttr(input, "level")) {
      descriptor->level = AttrAsUint32(input, "level");
    }
    // subIFD (OME-TIFF)
    if (HasAttr(input, "subifd")) {
      descriptor->subifd = AttrAsInt32(input, "subifd");
    }
    // Create new image
    if (HasAttr(input, "createChannels")) {
      descriptor->createChannels = AttrAsUint32(input, "createChannels");
      descriptor->createWidth = AttrAsUint32(input, "createWidth");
      descriptor->createHeight = AttrAsUint32(input, "createHeight");
      if (HasAttr(input, "createNoiseType")) {
        descriptor->createNoiseType = AttrAsStr(input, "createNoiseType");
        descriptor->createNoiseMean = AttrAsDouble(input, "createNoiseMean");
        descriptor->createNoiseSigma = AttrAsDouble(input, "createNoiseSigma");
      } else {
        descriptor->createBackground = AttrAsVectorOfDouble(input, "createBackground");
      }
    }
    // Limit input images to a given number of pixels, where pixels = width * height
    descriptor->limitInputPixels = AttrAsUint32(input, "limitInputPixels");
    // Allow switch from random to sequential access
    descriptor->access = AttrAsBool(input, "sequentialRead") ? VIPS_ACCESS_SEQUENTIAL : VIPS_ACCESS_RANDOM;
    // Remove safety features and allow unlimited SVG/PNG input
    descriptor->unlimited = AttrAsBool(input, "unlimited");
    return descriptor;
  }

// How many tasks are in the queue?
  volatile int counterQueue = 0;

  // How many tasks are being processed?
  volatile int counterProcess = 0;
}