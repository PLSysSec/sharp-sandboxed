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

#ifndef SRC_COMMON_HOST_H_
#define SRC_COMMON_HOST_H_

#include <string>

#include <napi.h>

#include "rlbox_mgr.h"

struct InputDescriptor;

namespace sharp {

  // Convenience methods to access the attributes of a Napi::Object
  bool HasAttr(Napi::Object obj, std::string attr);
  std::string AttrAsStr(Napi::Object obj, std::string attr);
  std::string AttrAsStr(Napi::Object obj, unsigned int const attr);
  uint32_t AttrAsUint32(Napi::Object obj, std::string attr);
  int32_t AttrAsInt32(Napi::Object obj, std::string attr);
  int32_t AttrAsInt32(Napi::Object obj, unsigned int const attr);
  double AttrAsDouble(Napi::Object obj, std::string attr);
  double AttrAsDouble(Napi::Object obj, unsigned int const attr);
  bool AttrAsBool(Napi::Object obj, std::string attr);
  std::vector<double> AttrAsVectorOfDouble(Napi::Object obj, std::string attr);
  std::vector<int32_t> AttrAsInt32Vector(Napi::Object obj, std::string attr);

  // Create an InputDescriptor instance from a Napi::Object describing an input image
  tainted_vips<InputDescriptor*> CreateInputDescriptor(rlbox_sandbox_vips* sandbox, Napi::Object input);


  // How many tasks are in the queue?
  extern volatile int counterQueue;

  // How many tasks are being processed?
  extern volatile int counterProcess;

  /*
    Called when a Buffer undergoes GC, required to support mixed runtime libraries in Windows
  */
  extern std::function<void(void*, char*)> FreeCallback;
  extern std::function<void(void*, char*)> DeleteCallback;

  /*
    Called with warnings from the glib-registered "VIPS" domain
  */
  void VipsWarningCallback(char const* log_domain, GLogLevelFlags log_level, char const* message, void* ignore);

  /*
    Pop the oldest warning message from the queue
  */
  std::string VipsWarningPop();

  /*
    Copy string to the given sandbox
  */
  tainted_vips<const char*> CopyStringToSandbox(rlbox_sandbox_vips* sandbox, const char* str);

  /*
    Copy vector to the given sandbox
  */
  template<typename T>
  tainted_vips<T*> CopyVectorToSandbox(rlbox_sandbox_vips* sandbox, std::vector<T> vec) {
    size_t size = vec.size();
    size_t non_zero_size = size == 0? 1 : size;
    tainted_vips<T*> t_buffer = sandbox->malloc_in_sandbox<T>(non_zero_size);
    for(size_t i = 0; i < size; i++) {
      t_buffer[i] = vec[i];
    }
    return t_buffer;
  }

  /*
    Call vips_enum_from_nick in the given sandbox
  */
  tainted_vips<int> SandboxVipsEnumFromNick(rlbox_sandbox_vips* sandbox, const char *domain, GType type, const char *str);

  /*
    call vips_enum_nick in the given sandbox
  */
  std::string SandboxVipsEnumNick(rlbox_sandbox_vips* sandbox, GType enm, tainted_vips<int> value);


}

#endif  // SRC_COMMON_HOST_H_
