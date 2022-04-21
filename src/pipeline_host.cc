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

#include <algorithm>
#include <cmath>
#include <map>
#include <memory>
#include <numeric>
#include <string>
#include <tuple>
#include <utility>
#include <vector>
#include <sys/types.h>
#include <sys/stat.h>

#include <vips/vips8>
#include <napi.h>

#include "common_host.h"
#include "common_sandbox.h"
#include "operations.h"
#include "pipeline_host.h"
#include "pipeline_sandbox.h"
#include "rlbox_mgr.h"

#if defined(WIN32)
#define STAT64_STRUCT __stat64
#define STAT64_FUNCTION _stat64
#elif defined(__APPLE__)
#define STAT64_STRUCT stat
#define STAT64_FUNCTION stat
#elif defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__) || defined(__DragonFly__)
#define STAT64_STRUCT stat
#define STAT64_FUNCTION stat
#else
#define STAT64_STRUCT stat64
#define STAT64_FUNCTION stat64
#endif


static const char configs_only_reason [] = "condition only controls internal configs";

class PipelineWorker : public Napi::AsyncWorker {
 public:
  PipelineWorker(Napi::Function callback, tainted_vips<PipelineBaton*> t_baton,
    Napi::Function debuglog, Napi::Function queueListener, rlbox_sandbox_vips* sandbox) :
    Napi::AsyncWorker(callback),
    t_baton(t_baton),
    debuglog(Napi::Persistent(debuglog)),
    queueListener(Napi::Persistent(queueListener)),
    sandbox(sandbox) {}
  ~PipelineWorker() {}

  // libuv worker
  void Execute() {
    // Decrement queued task counter
    g_atomic_int_dec_and_test(&sharp::counterQueue);
    // Increment processing task counter
    g_atomic_int_inc(&sharp::counterProcess);

    sandbox->invoke_sandbox_function(PipelineWorkerExecute, t_baton);
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

    std::string errString = sandbox->invoke_sandbox_function(PipelineBaton_GetErr, t_baton).copy_and_verify_string([](std::string val) {
      // Worst case, the library says there is an error when there isn't
      return val;
    });

    const char image_attrib_reason[] = "Reading attributes of the image for the first and only time.";

    if (errString == std::string("")) {
      tainted_vips<int> width = sandbox->invoke_sandbox_function(PipelineBaton_GetWidth, t_baton);
      tainted_vips<int> height = sandbox->invoke_sandbox_function(PipelineBaton_GetHeight, t_baton);
      if (sandbox->invoke_sandbox_function(PipelineBaton_GetTopOffsetPre, t_baton).unverified_safe_because(image_attrib_reason) != -1 &&
            (sandbox->invoke_sandbox_function(PipelineBaton_GetWidth, t_baton).unverified_safe_because(image_attrib_reason) == -1 ||
            sandbox->invoke_sandbox_function(PipelineBaton_GetHeight, t_baton).unverified_safe_because(image_attrib_reason) == -1))
      {
        width = sandbox->invoke_sandbox_function(PipelineBaton_GetWidthPre, t_baton);
        height = sandbox->invoke_sandbox_function(PipelineBaton_GetHeightPre, t_baton);
      }
      if (sandbox->invoke_sandbox_function(PipelineBaton_GetTopOffsetPost, t_baton).unverified_safe_because(image_attrib_reason) != -1) {
        width = sandbox->invoke_sandbox_function(PipelineBaton_GetWidthPost, t_baton);
        height = sandbox->invoke_sandbox_function(PipelineBaton_GetHeightPost, t_baton);
      }
      // Info Object
      Napi::Object info = Napi::Object::New(env);
      auto formatString = sandbox->invoke_sandbox_function(PipelineBaton_GetFormatOut, t_baton)
        .copy_and_verify_string([](std::string val) {
          // UNSAFE --- sanity check?
          return val;
        });

      info.Set("format", formatString.c_str());
      info.Set("width", static_cast<uint32_t>(width.unverified_safe_because(image_attrib_reason)));
      info.Set("height", static_cast<uint32_t>(height.unverified_safe_because(image_attrib_reason)));
      info.Set("channels", static_cast<uint32_t>(sandbox->invoke_sandbox_function(PipelineBaton_GetChannels, t_baton).unverified_safe_because(image_attrib_reason)));
      if (sandbox->invoke_sandbox_function(PipelineBaton_GetFormatOut, t_baton).unverified_safe_pointer_because(4, configs_only_reason) == std::string("raw")) {
        tainted_vips<VipsBandFormat> raw_depth = sandbox->invoke_sandbox_function(PipelineBaton_GetRawDepth, t_baton);
        info.Set("depth", sharp::SandboxVipsEnumNick(sandbox, VIPS_TYPE_BAND_FORMAT, rlbox::sandbox_static_cast<int>(raw_depth)));
      }
      info.Set("premultiplied", sandbox->invoke_sandbox_function(PipelineBaton_GetPremultiplied, t_baton).unverified_safe_because(image_attrib_reason));
      if (sandbox->invoke_sandbox_function(PipelineBaton_GetHasCropOffset, t_baton).unverified_safe_because(configs_only_reason)) {
        info.Set("cropOffsetLeft", static_cast<int32_t>(sandbox->invoke_sandbox_function(PipelineBaton_GetCropOffsetLeft, t_baton).unverified_safe_because(image_attrib_reason)));
        info.Set("cropOffsetTop", static_cast<int32_t>(sandbox->invoke_sandbox_function(PipelineBaton_GetCropOffsetTop, t_baton).unverified_safe_because(image_attrib_reason)));
      }
      if (sandbox->invoke_sandbox_function(PipelineBaton_GetTrimThreshold, t_baton).unverified_safe_because(configs_only_reason) > 0.0) {
        info.Set("trimOffsetLeft", static_cast<int32_t>(sandbox->invoke_sandbox_function(PipelineBaton_GetTrimOffsetLeft, t_baton).unverified_safe_because(image_attrib_reason)));
        info.Set("trimOffsetTop", static_cast<int32_t>(sandbox->invoke_sandbox_function(PipelineBaton_GetTrimOffsetTop, t_baton).unverified_safe_because(image_attrib_reason)));
      }

      uint32_t outBufferLength = static_cast<uint32_t>(sandbox->invoke_sandbox_function(PipelineBaton_GetBufferOutLength, t_baton).unverified_safe_because(image_attrib_reason));
      if (outBufferLength > 0) {
        // Add buffer size to info
        info.Set("size", outBufferLength);
        tainted_vips<char*> t_buffer_ref = rlbox::sandbox_static_cast<char*>(sandbox->invoke_sandbox_function(PipelineBaton_GetBufferOut, t_baton));
        char* buffer_ref = t_buffer_ref.copy_and_verify_range(
          [](std::unique_ptr<char[]> val) {
          return val.release();
        }, outBufferLength);
        sandbox->free_in_sandbox(t_buffer_ref);

        // Pass ownership of output data to Buffer instance
        Napi::Buffer<char> data = Napi::Buffer<char>::New(env, buffer_ref,
          outBufferLength, sharp::DeleteCallback);
        Callback().MakeCallback(Receiver().Value(), { env.Null(), data, info });
      } else {
        // Add file size to info
        struct STAT64_STRUCT st;
        const char* file = sandbox->invoke_sandbox_function(PipelineBaton_GetFileOut, t_baton).UNSAFE_unverified();
        if (STAT64_FUNCTION(file, &st) == 0) {
          info.Set("size", static_cast<uint32_t>(st.st_size));
        }
        Callback().MakeCallback(Receiver().Value(), { env.Null(), info });
      }
    } else {
      auto errString = sandbox->invoke_sandbox_function(PipelineBaton_GetErr, t_baton)
      .copy_and_verify_string([](std::string val) {
        // Worst case you'd get a bad error message
        return val;
      });
      Callback().MakeCallback(Receiver().Value(), { Napi::Error::New(env, errString.c_str()).Value() });
    }

    // Delete baton
    sandbox->invoke_sandbox_function(DestroyPipelineBaton, t_baton);

    // Decrement processing task counter
    g_atomic_int_dec_and_test(&sharp::counterProcess);
    Napi::Number queueLength = Napi::Number::New(env, static_cast<double>(sharp::counterQueue));
    queueListener.Call(Receiver().Value(), { queueLength });
  }

 private:
  tainted_vips<PipelineBaton*> t_baton;
  Napi::FunctionReference debuglog;
  Napi::FunctionReference queueListener;
  rlbox_sandbox_vips* sandbox;
};

/*
  pipeline(options, output, callback)
*/
Napi::Value pipeline(const Napi::CallbackInfo& info) {
  rlbox_sandbox_vips* sandbox = GetVipsSandbox();

  // V8 objects are converted to non-V8 types held in the baton struct
  tainted_vips<PipelineBaton*> t_baton = sandbox->invoke_sandbox_function(CreatePipelineBaton);
  PipelineBaton* baton = t_baton.UNSAFE_unverified();
  Napi::Object options = info[0].As<Napi::Object>();

  // Input
  tainted_vips<InputDescriptor*> inputdesc = sharp::CreateInputDescriptor(sandbox, options.Get("input").As<Napi::Object>());
  sandbox->invoke_sandbox_function(PipelineBaton_SetInput, t_baton, inputdesc);
  // Extract image options
  sandbox->invoke_sandbox_function(PipelineBaton_SetTopOffsetPre, t_baton, sharp::AttrAsInt32(options, "topOffsetPre"));
  sandbox->invoke_sandbox_function(PipelineBaton_SetLeftOffsetPre, t_baton, sharp::AttrAsInt32(options, "leftOffsetPre"));
  sandbox->invoke_sandbox_function(PipelineBaton_SetWidthPre, t_baton, sharp::AttrAsInt32(options, "widthPre"));
  sandbox->invoke_sandbox_function(PipelineBaton_SetHeightPre, t_baton, sharp::AttrAsInt32(options, "heightPre"));
  sandbox->invoke_sandbox_function(PipelineBaton_SetTopOffsetPost, t_baton, sharp::AttrAsInt32(options, "topOffsetPost"));
  sandbox->invoke_sandbox_function(PipelineBaton_SetLeftOffsetPost, t_baton, sharp::AttrAsInt32(options, "leftOffsetPost"));
  sandbox->invoke_sandbox_function(PipelineBaton_SetWidthPost, t_baton, sharp::AttrAsInt32(options, "widthPost"));
  sandbox->invoke_sandbox_function(PipelineBaton_SetHeightPost, t_baton, sharp::AttrAsInt32(options, "heightPost"));
  // Output image dimensions
  sandbox->invoke_sandbox_function(PipelineBaton_SetWidth, t_baton, sharp::AttrAsInt32(options, "width"));
  sandbox->invoke_sandbox_function(PipelineBaton_SetHeight, t_baton, sharp::AttrAsInt32(options, "height"));
  // Canvas option
  std::string canvas = sharp::AttrAsStr(options, "canvas");
  if (canvas == "crop") {
    sandbox->invoke_sandbox_function(PipelineBaton_SetCanvas, t_baton, sharp::Canvas::CROP);
  } else if (canvas == "embed") {
    sandbox->invoke_sandbox_function(PipelineBaton_SetCanvas, t_baton, sharp::Canvas::EMBED);
  } else if (canvas == "max") {
    sandbox->invoke_sandbox_function(PipelineBaton_SetCanvas, t_baton, sharp::Canvas::MAX);
  } else if (canvas == "min") {
    sandbox->invoke_sandbox_function(PipelineBaton_SetCanvas, t_baton, sharp::Canvas::MIN);
  } else if (canvas == "ignore_aspect") {
    sandbox->invoke_sandbox_function(PipelineBaton_SetCanvas, t_baton, sharp::Canvas::IGNORE_ASPECT);
  }
  // Tint chroma
  sandbox->invoke_sandbox_function(PipelineBaton_SetTintA, t_baton, sharp::AttrAsDouble(options, "tintA"));
  sandbox->invoke_sandbox_function(PipelineBaton_SetTintB, t_baton, sharp::AttrAsDouble(options, "tintB"));
  // Composite
  Napi::Array compositeArray = options.Get("composite").As<Napi::Array>();
  for (unsigned int i = 0; i < compositeArray.Length(); i++) {
    Napi::Object compositeObject = compositeArray.Get(i).As<Napi::Object>();
    tainted_vips<Composite*> composite = sandbox->invoke_sandbox_function(CreateComposite);
    sandbox->invoke_sandbox_function(Composite_SetInput, composite, sharp::CreateInputDescriptor(sandbox, compositeObject.Get("input").As<Napi::Object>()));
    sandbox->invoke_sandbox_function(Composite_SetMode, composite, rlbox::sandbox_static_cast<VipsBlendMode>(
      sharp::SandboxVipsEnumFromNick(sandbox, nullptr, VIPS_TYPE_BLEND_MODE, sharp::AttrAsStr(compositeObject, "blend").data())));
    sandbox->invoke_sandbox_function(Composite_SetGravity, composite, sharp::AttrAsUint32(compositeObject, "gravity"));
    sandbox->invoke_sandbox_function(Composite_SetLeft, composite, sharp::AttrAsInt32(compositeObject, "left"));
    sandbox->invoke_sandbox_function(Composite_SetTop, composite, sharp::AttrAsInt32(compositeObject, "top"));
    sandbox->invoke_sandbox_function(Composite_SetHasOffset, composite, sharp::AttrAsBool(compositeObject, "hasOffset"));
    sandbox->invoke_sandbox_function(Composite_SetTile, composite, sharp::AttrAsBool(compositeObject, "tile"));
    sandbox->invoke_sandbox_function(Composite_SetPremultiplied, composite, sharp::AttrAsBool(compositeObject, "premultiplied"));
    sandbox->invoke_sandbox_function(PipelineBaton_Composite_PushBack, t_baton, composite);
  }
  // Resize options
  sandbox->invoke_sandbox_function(PipelineBaton_SetWithoutEnlargement, t_baton, sharp::AttrAsBool(options, "withoutEnlargement"));
  sandbox->invoke_sandbox_function(PipelineBaton_SetWithoutReduction, t_baton, sharp::AttrAsBool(options, "withoutReduction"));
  sandbox->invoke_sandbox_function(PipelineBaton_SetPosition, t_baton, sharp::AttrAsInt32(options, "position"));
  PipelineBaton_SetResizeBackground(baton, sharp::AttrAsVectorOfDouble(options, "resizeBackground"));
  PipelineBaton_SetKernel(baton, sharp::AttrAsStr(options, "kernel").c_str());
  sandbox->invoke_sandbox_function(PipelineBaton_SetFastShrinkOnLoad, t_baton, sharp::AttrAsBool(options, "fastShrinkOnLoad"));
  // Join Channel Options
  if (options.Has("joinChannelIn")) {
    Napi::Array joinChannelArray = options.Get("joinChannelIn").As<Napi::Array>();
    for (unsigned int i = 0; i < joinChannelArray.Length(); i++) {
      sandbox->invoke_sandbox_function(PipelineBaton_JoinChannelIn_PushBack, t_baton,
        sharp::CreateInputDescriptor(sandbox, joinChannelArray.Get(i).As<Napi::Object>()));
    }
  }
  // Operators
  sandbox->invoke_sandbox_function(PipelineBaton_SetFlatten, t_baton, sharp::AttrAsBool(options, "flatten"));
  PipelineBaton_SetFlattenBackground(baton, sharp::AttrAsVectorOfDouble(options, "flattenBackground"));
  sandbox->invoke_sandbox_function(PipelineBaton_SetNegate, t_baton, sharp::AttrAsBool(options, "negate"));
  sandbox->invoke_sandbox_function(PipelineBaton_SetNegateAlpha, t_baton, sharp::AttrAsBool(options, "negateAlpha"));
  sandbox->invoke_sandbox_function(PipelineBaton_SetBlurSigma, t_baton, sharp::AttrAsDouble(options, "blurSigma"));
  sandbox->invoke_sandbox_function(PipelineBaton_SetBrightness, t_baton, sharp::AttrAsDouble(options, "brightness"));
  sandbox->invoke_sandbox_function(PipelineBaton_SetSaturation, t_baton, sharp::AttrAsDouble(options, "saturation"));
  sandbox->invoke_sandbox_function(PipelineBaton_SetHue, t_baton, sharp::AttrAsInt32(options, "hue"));
  sandbox->invoke_sandbox_function(PipelineBaton_SetLightness, t_baton, sharp::AttrAsDouble(options, "lightness"));
  sandbox->invoke_sandbox_function(PipelineBaton_SetMedianSize, t_baton, sharp::AttrAsUint32(options, "medianSize"));
  sandbox->invoke_sandbox_function(PipelineBaton_SetSharpenSigma, t_baton, sharp::AttrAsDouble(options, "sharpenSigma"));
  sandbox->invoke_sandbox_function(PipelineBaton_SetSharpenM1, t_baton, sharp::AttrAsDouble(options, "sharpenM1"));
  sandbox->invoke_sandbox_function(PipelineBaton_SetSharpenM2, t_baton, sharp::AttrAsDouble(options, "sharpenM2"));
  sandbox->invoke_sandbox_function(PipelineBaton_SetSharpenX1, t_baton, sharp::AttrAsDouble(options, "sharpenX1"));
  sandbox->invoke_sandbox_function(PipelineBaton_SetSharpenY2, t_baton, sharp::AttrAsDouble(options, "sharpenY2"));
  sandbox->invoke_sandbox_function(PipelineBaton_SetSharpenY3, t_baton, sharp::AttrAsDouble(options, "sharpenY3"));
  sandbox->invoke_sandbox_function(PipelineBaton_SetThreshold, t_baton, sharp::AttrAsInt32(options, "threshold"));
  sandbox->invoke_sandbox_function(PipelineBaton_SetThresholdGrayscale, t_baton, sharp::AttrAsBool(options, "thresholdGrayscale"));
  sandbox->invoke_sandbox_function(PipelineBaton_SetTrimThreshold, t_baton, sharp::AttrAsDouble(options, "trimThreshold"));
  sandbox->invoke_sandbox_function(PipelineBaton_SetGamma, t_baton, sharp::AttrAsDouble(options, "gamma"));
  sandbox->invoke_sandbox_function(PipelineBaton_SetGammaOut, t_baton, sharp::AttrAsDouble(options, "gammaOut"));
  sandbox->invoke_sandbox_function(PipelineBaton_SetLinearA, t_baton, sharp::AttrAsDouble(options, "linearA"));
  sandbox->invoke_sandbox_function(PipelineBaton_SetLinearB, t_baton, sharp::AttrAsDouble(options, "linearB"));
  sandbox->invoke_sandbox_function(PipelineBaton_SetGreyscale, t_baton, sharp::AttrAsBool(options, "greyscale"));
  sandbox->invoke_sandbox_function(PipelineBaton_SetNormalise, t_baton, sharp::AttrAsBool(options, "normalise"));
  sandbox->invoke_sandbox_function(PipelineBaton_SetClaheWidth, t_baton, sharp::AttrAsUint32(options, "claheWidth"));
  sandbox->invoke_sandbox_function(PipelineBaton_SetClaheHeight, t_baton, sharp::AttrAsUint32(options, "claheHeight"));
  sandbox->invoke_sandbox_function(PipelineBaton_SetClaheMaxSlope, t_baton, sharp::AttrAsUint32(options, "claheMaxSlope"));
  sandbox->invoke_sandbox_function(PipelineBaton_SetUseExifOrientation, t_baton, sharp::AttrAsBool(options, "useExifOrientation"));
  sandbox->invoke_sandbox_function(PipelineBaton_SetAngle, t_baton, sharp::AttrAsInt32(options, "angle"));
  sandbox->invoke_sandbox_function(PipelineBaton_SetRotationAngle, t_baton, sharp::AttrAsDouble(options, "rotationAngle"));
  PipelineBaton_SetRotationBackground(baton, sharp::AttrAsVectorOfDouble(options, "rotationBackground"));
  sandbox->invoke_sandbox_function(PipelineBaton_SetRotateBeforePreExtract, t_baton, sharp::AttrAsBool(options, "rotateBeforePreExtract"));
  sandbox->invoke_sandbox_function(PipelineBaton_SetFlip, t_baton, sharp::AttrAsBool(options, "flip"));
  sandbox->invoke_sandbox_function(PipelineBaton_SetFlop, t_baton, sharp::AttrAsBool(options, "flop"));
  sandbox->invoke_sandbox_function(PipelineBaton_SetExtendTop, t_baton, sharp::AttrAsInt32(options, "extendTop"));
  sandbox->invoke_sandbox_function(PipelineBaton_SetExtendBottom, t_baton, sharp::AttrAsInt32(options, "extendBottom"));
  sandbox->invoke_sandbox_function(PipelineBaton_SetExtendLeft, t_baton, sharp::AttrAsInt32(options, "extendLeft"));
  sandbox->invoke_sandbox_function(PipelineBaton_SetExtendRight, t_baton, sharp::AttrAsInt32(options, "extendRight"));
  PipelineBaton_SetExtendBackground(baton, sharp::AttrAsVectorOfDouble(options, "extendBackground"));
  sandbox->invoke_sandbox_function(PipelineBaton_SetExtractChannel, t_baton, sharp::AttrAsInt32(options, "extractChannel"));
  PipelineBaton_SetAffineMatrix(baton, sharp::AttrAsVectorOfDouble(options, "affineMatrix"));
  PipelineBaton_SetAffineBackground(baton, sharp::AttrAsVectorOfDouble(options, "affineBackground"));
  sandbox->invoke_sandbox_function(PipelineBaton_SetAffineIdx, t_baton, sharp::AttrAsDouble(options, "affineIdx"));
  sandbox->invoke_sandbox_function(PipelineBaton_SetAffineIdy, t_baton, sharp::AttrAsDouble(options, "affineIdy"));
  sandbox->invoke_sandbox_function(PipelineBaton_SetAffineOdx, t_baton, sharp::AttrAsDouble(options, "affineOdx"));
  sandbox->invoke_sandbox_function(PipelineBaton_SetAffineOdy, t_baton, sharp::AttrAsDouble(options, "affineOdy"));
  PipelineBaton_SetAffineInterpolator(baton, sharp::AttrAsStr(options, "affineInterpolator").c_str());
  sandbox->invoke_sandbox_function(PipelineBaton_SetRemoveAlpha, t_baton, sharp::AttrAsBool(options, "removeAlpha"));
  sandbox->invoke_sandbox_function(PipelineBaton_SetEnsureAlpha, t_baton, sharp::AttrAsDouble(options, "ensureAlpha"));
  if (options.Has("boolean")) {
    sandbox->invoke_sandbox_function(PipelineBaton_SetBoolean, t_baton, sharp::CreateInputDescriptor(sandbox, options.Get("boolean").As<Napi::Object>()));
    sandbox->invoke_sandbox_function(PipelineBaton_SetBooleanOp, t_baton, sharp::GetBooleanOperation(sharp::AttrAsStr(options, "booleanOp")));
  }
  if (options.Has("bandBoolOp")) {
    sandbox->invoke_sandbox_function(PipelineBaton_SetBandBoolOp, t_baton, sharp::GetBooleanOperation(sharp::AttrAsStr(options, "bandBoolOp")));
  }
  if (options.Has("convKernel")) {
    Napi::Object kernel = options.Get("convKernel").As<Napi::Object>();
    sandbox->invoke_sandbox_function(PipelineBaton_SetConvKernelWidth, t_baton, sharp::AttrAsUint32(kernel, "width"));
    sandbox->invoke_sandbox_function(PipelineBaton_SetConvKernelHeight, t_baton, sharp::AttrAsUint32(kernel, "height"));
    sandbox->invoke_sandbox_function(PipelineBaton_SetConvKernelScale, t_baton, sharp::AttrAsDouble(kernel, "scale"));
    sandbox->invoke_sandbox_function(PipelineBaton_SetConvKernelOffset, t_baton, sharp::AttrAsDouble(kernel, "offset"));
    size_t const kernelSize = static_cast<size_t>(PipelineBaton_GetConvKernelWidth(baton) * PipelineBaton_GetConvKernelHeight(baton));
    PipelineBaton_SetConvKernel(baton, std::unique_ptr<double[]>(new double[kernelSize]));
    Napi::Array kdata = kernel.Get("kernel").As<Napi::Array>();
    for (unsigned int i = 0; i < kernelSize; i++) {
      sandbox->invoke_sandbox_function(PipelineBaton_GetConvKernel, t_baton)[i] = sharp::AttrAsDouble(kdata, i);
    }
  }
  if (options.Has("recombMatrix")) {
    PipelineBaton_SetRecombMatrix(baton, std::unique_ptr<double[]>(new double[9]));
    Napi::Array recombMatrix = options.Get("recombMatrix").As<Napi::Array>();
    for (unsigned int i = 0; i < 9; i++) {
      sandbox->invoke_sandbox_function(PipelineBaton_GetRecombMatrix, t_baton)[i] = sharp::AttrAsDouble(recombMatrix, i);
    }
  }
  sandbox->invoke_sandbox_function(PipelineBaton_SetColourspaceInput, t_baton, sharp::GetInterpretation(sharp::AttrAsStr(options, "colourspaceInput")));
  if (sandbox->invoke_sandbox_function(PipelineBaton_GetColourspaceInput, t_baton).unverified_safe_because("error checking") == VIPS_INTERPRETATION_ERROR) {
    sandbox->invoke_sandbox_function(PipelineBaton_SetColourspaceInput, t_baton, VIPS_INTERPRETATION_LAST);
  }
  sandbox->invoke_sandbox_function(PipelineBaton_SetColourspace, t_baton, sharp::GetInterpretation(sharp::AttrAsStr(options, "colourspace")));
  if (sandbox->invoke_sandbox_function(PipelineBaton_GetColourspace, t_baton).unverified_safe_because("error checking") == VIPS_INTERPRETATION_ERROR) {
    sandbox->invoke_sandbox_function(PipelineBaton_SetColourspace, t_baton, VIPS_INTERPRETATION_sRGB);
  }
  // Output
  PipelineBaton_SetFormatOut(baton, sharp::AttrAsStr(options, "formatOut").c_str());
  PipelineBaton_SetFileOut(baton, sharp::AttrAsStr(options, "fileOut").c_str());
  sandbox->invoke_sandbox_function(PipelineBaton_SetWithMetadata, t_baton, sharp::AttrAsBool(options, "withMetadata"));
  sandbox->invoke_sandbox_function(PipelineBaton_SetWithMetadataOrientation, t_baton, sharp::AttrAsUint32(options, "withMetadataOrientation"));
  sandbox->invoke_sandbox_function(PipelineBaton_SetWithMetadataDensity, t_baton, sharp::AttrAsDouble(options, "withMetadataDensity"));
  PipelineBaton_SetWithMetadataIcc(baton, sharp::AttrAsStr(options, "withMetadataIcc").c_str());
  Napi::Object mdStrs = options.Get("withMetadataStrs").As<Napi::Object>();
  Napi::Array mdStrKeys = mdStrs.GetPropertyNames();
  for (unsigned int i = 0; i < mdStrKeys.Length(); i++) {
    std::string k = sharp::AttrAsStr(mdStrKeys, i);
    PipelineBaton_WithMetadataStrs_Insert(baton, std::make_pair(k, sharp::AttrAsStr(mdStrs, k)));
  }
  sandbox->invoke_sandbox_function(PipelineBaton_SetTimeoutSeconds, t_baton, sharp::AttrAsUint32(options, "timeoutSeconds"));
  // Format-specific
  sandbox->invoke_sandbox_function(PipelineBaton_SetJpegQuality, t_baton, sharp::AttrAsUint32(options, "jpegQuality"));
  sandbox->invoke_sandbox_function(PipelineBaton_SetJpegProgressive, t_baton, sharp::AttrAsBool(options, "jpegProgressive"));
  PipelineBaton_SetJpegChromaSubsampling(baton, sharp::AttrAsStr(options, "jpegChromaSubsampling").c_str());
  sandbox->invoke_sandbox_function(PipelineBaton_SetJpegTrellisQuantisation, t_baton, sharp::AttrAsBool(options, "jpegTrellisQuantisation"));
  sandbox->invoke_sandbox_function(PipelineBaton_SetJpegQuantisationTable, t_baton, sharp::AttrAsUint32(options, "jpegQuantisationTable"));
  sandbox->invoke_sandbox_function(PipelineBaton_SetJpegOvershootDeringing, t_baton, sharp::AttrAsBool(options, "jpegOvershootDeringing"));
  sandbox->invoke_sandbox_function(PipelineBaton_SetJpegOptimiseScans, t_baton, sharp::AttrAsBool(options, "jpegOptimiseScans"));
  sandbox->invoke_sandbox_function(PipelineBaton_SetJpegOptimiseCoding, t_baton, sharp::AttrAsBool(options, "jpegOptimiseCoding"));
  sandbox->invoke_sandbox_function(PipelineBaton_SetPngProgressive, t_baton, sharp::AttrAsBool(options, "pngProgressive"));
  sandbox->invoke_sandbox_function(PipelineBaton_SetPngCompressionLevel, t_baton, sharp::AttrAsUint32(options, "pngCompressionLevel"));
  sandbox->invoke_sandbox_function(PipelineBaton_SetPngAdaptiveFiltering, t_baton, sharp::AttrAsBool(options, "pngAdaptiveFiltering"));
  sandbox->invoke_sandbox_function(PipelineBaton_SetPngPalette, t_baton, sharp::AttrAsBool(options, "pngPalette"));
  sandbox->invoke_sandbox_function(PipelineBaton_SetPngQuality, t_baton, sharp::AttrAsUint32(options, "pngQuality"));
  sandbox->invoke_sandbox_function(PipelineBaton_SetPngEffort, t_baton, sharp::AttrAsUint32(options, "pngEffort"));
  sandbox->invoke_sandbox_function(PipelineBaton_SetPngBitdepth, t_baton, sharp::AttrAsUint32(options, "pngBitdepth"));
  sandbox->invoke_sandbox_function(PipelineBaton_SetPngDither, t_baton, sharp::AttrAsDouble(options, "pngDither"));
  sandbox->invoke_sandbox_function(PipelineBaton_SetJp2Quality, t_baton, sharp::AttrAsUint32(options, "jp2Quality"));
  sandbox->invoke_sandbox_function(PipelineBaton_SetJp2Lossless, t_baton, sharp::AttrAsBool(options, "jp2Lossless"));
  sandbox->invoke_sandbox_function(PipelineBaton_SetJp2TileHeight, t_baton, sharp::AttrAsUint32(options, "jp2TileHeight"));
  sandbox->invoke_sandbox_function(PipelineBaton_SetJp2TileWidth, t_baton, sharp::AttrAsUint32(options, "jp2TileWidth"));
  PipelineBaton_SetJp2ChromaSubsampling(baton, sharp::AttrAsStr(options, "jp2ChromaSubsampling").c_str());
  sandbox->invoke_sandbox_function(PipelineBaton_SetWebpQuality, t_baton, sharp::AttrAsUint32(options, "webpQuality"));
  sandbox->invoke_sandbox_function(PipelineBaton_SetWebpAlphaQuality, t_baton, sharp::AttrAsUint32(options, "webpAlphaQuality"));
  sandbox->invoke_sandbox_function(PipelineBaton_SetWebpLossless, t_baton, sharp::AttrAsBool(options, "webpLossless"));
  sandbox->invoke_sandbox_function(PipelineBaton_SetWebpNearLossless, t_baton, sharp::AttrAsBool(options, "webpNearLossless"));
  sandbox->invoke_sandbox_function(PipelineBaton_SetWebpSmartSubsample, t_baton, sharp::AttrAsBool(options, "webpSmartSubsample"));
  sandbox->invoke_sandbox_function(PipelineBaton_SetWebpEffort, t_baton, sharp::AttrAsUint32(options, "webpEffort"));
  sandbox->invoke_sandbox_function(PipelineBaton_SetGifBitdepth, t_baton, sharp::AttrAsUint32(options, "gifBitdepth"));
  sandbox->invoke_sandbox_function(PipelineBaton_SetGifEffort, t_baton, sharp::AttrAsUint32(options, "gifEffort"));
  sandbox->invoke_sandbox_function(PipelineBaton_SetGifDither, t_baton, sharp::AttrAsDouble(options, "gifDither"));
  sandbox->invoke_sandbox_function(PipelineBaton_SetTiffQuality, t_baton, sharp::AttrAsUint32(options, "tiffQuality"));
  sandbox->invoke_sandbox_function(PipelineBaton_SetTiffPyramid, t_baton, sharp::AttrAsBool(options, "tiffPyramid"));
  sandbox->invoke_sandbox_function(PipelineBaton_SetTiffBitdepth, t_baton, sharp::AttrAsUint32(options, "tiffBitdepth"));
  sandbox->invoke_sandbox_function(PipelineBaton_SetTiffTile, t_baton, sharp::AttrAsBool(options, "tiffTile"));
  sandbox->invoke_sandbox_function(PipelineBaton_SetTiffTileWidth, t_baton, sharp::AttrAsUint32(options, "tiffTileWidth"));
  sandbox->invoke_sandbox_function(PipelineBaton_SetTiffTileHeight, t_baton, sharp::AttrAsUint32(options, "tiffTileHeight"));
  sandbox->invoke_sandbox_function(PipelineBaton_SetTiffXres, t_baton, sharp::AttrAsDouble(options, "tiffXres"));
  sandbox->invoke_sandbox_function(PipelineBaton_SetTiffYres, t_baton, sharp::AttrAsDouble(options, "tiffYres"));

  if (sandbox->invoke_sandbox_function(PipelineBaton_GetTiffXres, t_baton).unverified_safe_because(configs_only_reason) == 1.0 &&
    sandbox->invoke_sandbox_function(PipelineBaton_GetTiffYres, t_baton).unverified_safe_because(configs_only_reason) == 1.0 &&
    sandbox->invoke_sandbox_function(PipelineBaton_GetWithMetadataDensity, t_baton).unverified_safe_because(configs_only_reason) > 0)
  {
    auto val = sandbox->invoke_sandbox_function(PipelineBaton_GetWithMetadataDensity, t_baton) / 25.4;
    sandbox->invoke_sandbox_function(PipelineBaton_SetTiffXres, t_baton, val);
    sandbox->invoke_sandbox_function(PipelineBaton_SetTiffYres, t_baton, val);
  }
  // tiff compression options
  sandbox->invoke_sandbox_function(PipelineBaton_SetTiffCompression, t_baton, rlbox::sandbox_static_cast<VipsForeignTiffCompression>(
  sharp::SandboxVipsEnumFromNick(sandbox, nullptr, VIPS_TYPE_FOREIGN_TIFF_COMPRESSION,
    sharp::AttrAsStr(options, "tiffCompression").data())));
  sandbox->invoke_sandbox_function(PipelineBaton_SetTiffPredictor, t_baton, rlbox::sandbox_static_cast<VipsForeignTiffPredictor>(
  sharp::SandboxVipsEnumFromNick(sandbox, nullptr, VIPS_TYPE_FOREIGN_TIFF_PREDICTOR,
    sharp::AttrAsStr(options, "tiffPredictor").data())));
  sandbox->invoke_sandbox_function(PipelineBaton_SetTiffResolutionUnit, t_baton, rlbox::sandbox_static_cast<VipsForeignTiffResunit>(
  sharp::SandboxVipsEnumFromNick(sandbox, nullptr, VIPS_TYPE_FOREIGN_TIFF_RESUNIT,
    sharp::AttrAsStr(options, "tiffResolutionUnit").data())));

  sandbox->invoke_sandbox_function(PipelineBaton_SetHeifQuality, t_baton, sharp::AttrAsUint32(options, "heifQuality"));
  sandbox->invoke_sandbox_function(PipelineBaton_SetHeifLossless, t_baton, sharp::AttrAsBool(options, "heifLossless"));
  sandbox->invoke_sandbox_function(PipelineBaton_SetHeifCompression, t_baton, rlbox::sandbox_static_cast<VipsForeignHeifCompression>(
    sharp::SandboxVipsEnumFromNick(sandbox, nullptr, VIPS_TYPE_FOREIGN_HEIF_COMPRESSION,
    sharp::AttrAsStr(options, "heifCompression").data())));
  sandbox->invoke_sandbox_function(PipelineBaton_SetHeifEffort, t_baton, sharp::AttrAsUint32(options, "heifEffort"));
  PipelineBaton_SetHeifChromaSubsampling(baton, sharp::AttrAsStr(options, "heifChromaSubsampling").c_str());
  // Raw output
  sandbox->invoke_sandbox_function(PipelineBaton_SetRawDepth, t_baton, rlbox::sandbox_static_cast<VipsBandFormat>(
    sharp::SandboxVipsEnumFromNick(sandbox, nullptr, VIPS_TYPE_BAND_FORMAT,
    sharp::AttrAsStr(options, "rawDepth").data())));
  // Animated output properties
  if (sharp::HasAttr(options, "loop")) {
    sandbox->invoke_sandbox_function(PipelineBaton_SetLoop, t_baton, sharp::AttrAsUint32(options, "loop"));
  }
  if (sharp::HasAttr(options, "delay")) {
    PipelineBaton_SetDelay(baton, sharp::AttrAsInt32Vector(options, "delay"));
  }
  // Tile output
  sandbox->invoke_sandbox_function(PipelineBaton_SetTileSize, t_baton, sharp::AttrAsUint32(options, "tileSize"));
  sandbox->invoke_sandbox_function(PipelineBaton_SetTileOverlap, t_baton, sharp::AttrAsUint32(options, "tileOverlap"));
  sandbox->invoke_sandbox_function(PipelineBaton_SetTileAngle, t_baton, sharp::AttrAsInt32(options, "tileAngle"));
  PipelineBaton_SetTileBackground(baton, sharp::AttrAsVectorOfDouble(options, "tileBackground"));
  sandbox->invoke_sandbox_function(PipelineBaton_SetTileSkipBlanks, t_baton, sharp::AttrAsInt32(options, "tileSkipBlanks"));
  sandbox->invoke_sandbox_function(PipelineBaton_SetTileContainer, t_baton, rlbox::sandbox_static_cast<VipsForeignDzContainer>(
    sharp::SandboxVipsEnumFromNick(sandbox, nullptr, VIPS_TYPE_FOREIGN_DZ_CONTAINER,
    sharp::AttrAsStr(options, "tileContainer").data())));
  sandbox->invoke_sandbox_function(PipelineBaton_SetTileLayout, t_baton, rlbox::sandbox_static_cast<VipsForeignDzLayout>(
    sharp::SandboxVipsEnumFromNick(sandbox, nullptr, VIPS_TYPE_FOREIGN_DZ_LAYOUT,
    sharp::AttrAsStr(options, "tileLayout").data())));
  PipelineBaton_SetTileFormat(baton, sharp::AttrAsStr(options, "tileFormat").c_str());
  sandbox->invoke_sandbox_function(PipelineBaton_SetTileDepth, t_baton, rlbox::sandbox_static_cast<VipsForeignDzDepth>(
    sharp::SandboxVipsEnumFromNick(sandbox, nullptr, VIPS_TYPE_FOREIGN_DZ_DEPTH,
    sharp::AttrAsStr(options, "tileDepth").data())));
  sandbox->invoke_sandbox_function(PipelineBaton_SetTileCentre, t_baton, sharp::AttrAsBool(options, "tileCentre"));
  PipelineBaton_SetTileId(baton, sharp::AttrAsStr(options, "tileId").c_str());

  // Force random access for certain operations
  tainted_vips<InputDescriptor*> input = sandbox->invoke_sandbox_function(PipelineBaton_GetInput, t_baton);
  if (sandbox->invoke_sandbox_function(InputDescriptor_GetAccess, input).unverified_safe_because(configs_only_reason) == VIPS_ACCESS_SEQUENTIAL) {
    if (
      sandbox->invoke_sandbox_function(PipelineBaton_GetTrimThreshold, t_baton).unverified_safe_because(configs_only_reason) > 0.0 ||
      sandbox->invoke_sandbox_function(PipelineBaton_GetNormalise, t_baton).unverified_safe_because(configs_only_reason) ||
      sandbox->invoke_sandbox_function(PipelineBaton_GetPosition, t_baton).unverified_safe_because(configs_only_reason) == 16 || sandbox->invoke_sandbox_function(PipelineBaton_GetPosition, t_baton).unverified_safe_because(configs_only_reason) == 17 ||
      sandbox->invoke_sandbox_function(PipelineBaton_GetAngle, t_baton).unverified_safe_because(configs_only_reason) % 360 != 0 ||
      fmod(sandbox->invoke_sandbox_function(PipelineBaton_GetRotationAngle, t_baton).unverified_safe_because(configs_only_reason), 360.0) != 0.0 ||
      sandbox->invoke_sandbox_function(PipelineBaton_GetUseExifOrientation, t_baton).unverified_safe_because(configs_only_reason)
    ) {
      sandbox->invoke_sandbox_function(InputDescriptor_SetAccess, input, VIPS_ACCESS_RANDOM);
    }
  }

  // Function to notify of libvips warnings
  Napi::Function debuglog = options.Get("debuglog").As<Napi::Function>();

  // Function to notify of queue length changes
  Napi::Function queueListener = options.Get("queueListener").As<Napi::Function>();

  // Join queue for worker thread
  Napi::Function callback = info[1].As<Napi::Function>();

  PipelineWorker *worker = new PipelineWorker(callback, t_baton, debuglog, queueListener, sandbox);
  worker->Receiver().Set("options", options);
  worker->Queue();

  // Increment queued task counter
  g_atomic_int_inc(&sharp::counterQueue);
  Napi::Number queueLength = Napi::Number::New(info.Env(), static_cast<double>(sharp::counterQueue));
  queueListener.Call(info.This(), { queueLength });

  return info.Env().Undefined();
}
