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

class PipelineWorker : public Napi::AsyncWorker {
 public:
  PipelineWorker(Napi::Function callback, PipelineBaton *baton,
    Napi::Function debuglog, Napi::Function queueListener) :
    Napi::AsyncWorker(callback),
    baton(baton),
    debuglog(Napi::Persistent(debuglog)),
    queueListener(Napi::Persistent(queueListener)) {}
  ~PipelineWorker() {}

  // libuv worker
  void Execute() {
    // Decrement queued task counter
    g_atomic_int_dec_and_test(&sharp::counterQueue);
    // Increment processing task counter
    g_atomic_int_inc(&sharp::counterProcess);

    PipelineWorkerExecute(baton);
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

    if (PipelineBaton_GetErr(baton) == std::string("")) {
      int width = PipelineBaton_GetWidth(baton);
      int height = PipelineBaton_GetHeight(baton);
      if (PipelineBaton_GetTopOffsetPre(baton) != -1 && (PipelineBaton_GetWidth(baton) == -1 || PipelineBaton_GetHeight(baton) == -1)) {
        width = PipelineBaton_GetWidthPre(baton);
        height = PipelineBaton_GetHeightPre(baton);
      }
      if (PipelineBaton_GetTopOffsetPost(baton) != -1) {
        width = PipelineBaton_GetWidthPost(baton);
        height = PipelineBaton_GetHeightPost(baton);
      }
      // Info Object
      Napi::Object info = Napi::Object::New(env);
      info.Set("format", PipelineBaton_GetFormatOut(baton));
      info.Set("width", static_cast<uint32_t>(width));
      info.Set("height", static_cast<uint32_t>(height));
      info.Set("channels", static_cast<uint32_t>(PipelineBaton_GetChannels(baton)));
      if (PipelineBaton_GetFormatOut(baton) == std::string("raw")) {
        info.Set("depth", vips_enum_nick(VIPS_TYPE_BAND_FORMAT, PipelineBaton_GetRawDepth(baton)));
      }
      info.Set("premultiplied", PipelineBaton_GetPremultiplied(baton));
      if (PipelineBaton_GetHasCropOffset(baton)) {
        info.Set("cropOffsetLeft", static_cast<int32_t>(PipelineBaton_GetCropOffsetLeft(baton)));
        info.Set("cropOffsetTop", static_cast<int32_t>(PipelineBaton_GetCropOffsetTop(baton)));
      }
      if (PipelineBaton_GetTrimThreshold(baton) > 0.0) {
        info.Set("trimOffsetLeft", static_cast<int32_t>(PipelineBaton_GetTrimOffsetLeft(baton)));
        info.Set("trimOffsetTop", static_cast<int32_t>(PipelineBaton_GetTrimOffsetTop(baton)));
      }

      if (PipelineBaton_GetBufferOutLength(baton) > 0) {
        // Add buffer size to info
        info.Set("size", static_cast<uint32_t>(PipelineBaton_GetBufferOutLength(baton)));
        // Pass ownership of output data to Buffer instance
        Napi::Buffer<char> data = Napi::Buffer<char>::New(env, static_cast<char*>(PipelineBaton_GetBufferOut(baton)),
          PipelineBaton_GetBufferOutLength(baton), sharp::FreeCallback);
        Callback().MakeCallback(Receiver().Value(), { env.Null(), data, info });
      } else {
        // Add file size to info
        struct STAT64_STRUCT st;
        if (STAT64_FUNCTION(PipelineBaton_GetFileOut(baton), &st) == 0) {
          info.Set("size", static_cast<uint32_t>(st.st_size));
        }
        Callback().MakeCallback(Receiver().Value(), { env.Null(), info });
      }
    } else {
      Callback().MakeCallback(Receiver().Value(), { Napi::Error::New(env, PipelineBaton_GetErr(baton)).Value() });
    }

    // Delete baton
    DestroyPipelineBaton(baton);

    // Decrement processing task counter
    g_atomic_int_dec_and_test(&sharp::counterProcess);
    Napi::Number queueLength = Napi::Number::New(env, static_cast<double>(sharp::counterQueue));
    queueListener.Call(Receiver().Value(), { queueLength });
  }

 private:
  PipelineBaton *baton;
  Napi::FunctionReference debuglog;
  Napi::FunctionReference queueListener;
};

/*
  pipeline(options, output, callback)
*/
Napi::Value pipeline(const Napi::CallbackInfo& info) {
  // V8 objects are converted to non-V8 types held in the baton struct
  PipelineBaton *baton = CreatePipelineBaton();
  Napi::Object options = info[0].As<Napi::Object>();

  // Input
  PipelineBaton_SetInput(baton, sharp::CreateInputDescriptor(options.Get("input").As<Napi::Object>()));
  // Extract image options
  PipelineBaton_SetTopOffsetPre(baton, sharp::AttrAsInt32(options, "topOffsetPre"));
  PipelineBaton_SetLeftOffsetPre(baton, sharp::AttrAsInt32(options, "leftOffsetPre"));
  PipelineBaton_SetWidthPre(baton, sharp::AttrAsInt32(options, "widthPre"));
  PipelineBaton_SetHeightPre(baton, sharp::AttrAsInt32(options, "heightPre"));
  PipelineBaton_SetTopOffsetPost(baton, sharp::AttrAsInt32(options, "topOffsetPost"));
  PipelineBaton_SetLeftOffsetPost(baton, sharp::AttrAsInt32(options, "leftOffsetPost"));
  PipelineBaton_SetWidthPost(baton, sharp::AttrAsInt32(options, "widthPost"));
  PipelineBaton_SetHeightPost(baton, sharp::AttrAsInt32(options, "heightPost"));
  // Output image dimensions
  PipelineBaton_SetWidth(baton, sharp::AttrAsInt32(options, "width"));
  PipelineBaton_SetHeight(baton, sharp::AttrAsInt32(options, "height"));
  // Canvas option
  std::string canvas = sharp::AttrAsStr(options, "canvas");
  if (canvas == "crop") {
    PipelineBaton_SetCanvas(baton, sharp::Canvas::CROP);
  } else if (canvas == "embed") {
    PipelineBaton_SetCanvas(baton, sharp::Canvas::EMBED);
  } else if (canvas == "max") {
    PipelineBaton_SetCanvas(baton, sharp::Canvas::MAX);
  } else if (canvas == "min") {
    PipelineBaton_SetCanvas(baton, sharp::Canvas::MIN);
  } else if (canvas == "ignore_aspect") {
    PipelineBaton_SetCanvas(baton, sharp::Canvas::IGNORE_ASPECT);
  }
  // Tint chroma
  PipelineBaton_SetTintA(baton, sharp::AttrAsDouble(options, "tintA"));
  PipelineBaton_SetTintB(baton, sharp::AttrAsDouble(options, "tintB"));
  // Composite
  Napi::Array compositeArray = options.Get("composite").As<Napi::Array>();
  for (unsigned int i = 0; i < compositeArray.Length(); i++) {
    Napi::Object compositeObject = compositeArray.Get(i).As<Napi::Object>();
    Composite *composite = new Composite;
    composite->input = sharp::CreateInputDescriptor(compositeObject.Get("input").As<Napi::Object>());
    composite->mode = static_cast<VipsBlendMode>(
      vips_enum_from_nick(nullptr, VIPS_TYPE_BLEND_MODE, sharp::AttrAsStr(compositeObject, "blend").data()));
    composite->gravity = sharp::AttrAsUint32(compositeObject, "gravity");
    composite->left = sharp::AttrAsInt32(compositeObject, "left");
    composite->top = sharp::AttrAsInt32(compositeObject, "top");
    composite->hasOffset = sharp::AttrAsBool(compositeObject, "hasOffset");
    composite->tile = sharp::AttrAsBool(compositeObject, "tile");
    composite->premultiplied = sharp::AttrAsBool(compositeObject, "premultiplied");
    PipelineBaton_Composite_PushBack(baton, composite);
  }
  // Resize options
  PipelineBaton_SetWithoutEnlargement(baton, sharp::AttrAsBool(options, "withoutEnlargement"));
  PipelineBaton_SetWithoutReduction(baton, sharp::AttrAsBool(options, "withoutReduction"));
  PipelineBaton_SetPosition(baton, sharp::AttrAsInt32(options, "position"));
  PipelineBaton_SetResizeBackground(baton, sharp::AttrAsVectorOfDouble(options, "resizeBackground"));
  PipelineBaton_SetKernel(baton, sharp::AttrAsStr(options, "kernel").c_str());
  PipelineBaton_SetFastShrinkOnLoad(baton, sharp::AttrAsBool(options, "fastShrinkOnLoad"));
  // Join Channel Options
  if (options.Has("joinChannelIn")) {
    Napi::Array joinChannelArray = options.Get("joinChannelIn").As<Napi::Array>();
    for (unsigned int i = 0; i < joinChannelArray.Length(); i++) {
      PipelineBaton_JoinChannelIn_PushBack(baton,
        sharp::CreateInputDescriptor(joinChannelArray.Get(i).As<Napi::Object>()));
    }
  }
  // Operators
  PipelineBaton_SetFlatten(baton, sharp::AttrAsBool(options, "flatten"));
  PipelineBaton_SetFlattenBackground(baton, sharp::AttrAsVectorOfDouble(options, "flattenBackground"));
  PipelineBaton_SetNegate(baton, sharp::AttrAsBool(options, "negate"));
  PipelineBaton_SetNegateAlpha(baton, sharp::AttrAsBool(options, "negateAlpha"));
  PipelineBaton_SetBlurSigma(baton, sharp::AttrAsDouble(options, "blurSigma"));
  PipelineBaton_SetBrightness(baton, sharp::AttrAsDouble(options, "brightness"));
  PipelineBaton_SetSaturation(baton, sharp::AttrAsDouble(options, "saturation"));
  PipelineBaton_SetHue(baton, sharp::AttrAsInt32(options, "hue"));
  PipelineBaton_SetLightness(baton, sharp::AttrAsDouble(options, "lightness"));
  PipelineBaton_SetMedianSize(baton, sharp::AttrAsUint32(options, "medianSize"));
  PipelineBaton_SetSharpenSigma(baton, sharp::AttrAsDouble(options, "sharpenSigma"));
  PipelineBaton_SetSharpenM1(baton, sharp::AttrAsDouble(options, "sharpenM1"));
  PipelineBaton_SetSharpenM2(baton, sharp::AttrAsDouble(options, "sharpenM2"));
  PipelineBaton_SetSharpenX1(baton, sharp::AttrAsDouble(options, "sharpenX1"));
  PipelineBaton_SetSharpenY2(baton, sharp::AttrAsDouble(options, "sharpenY2"));
  PipelineBaton_SetSharpenY3(baton, sharp::AttrAsDouble(options, "sharpenY3"));
  PipelineBaton_SetThreshold(baton, sharp::AttrAsInt32(options, "threshold"));
  PipelineBaton_SetThresholdGrayscale(baton, sharp::AttrAsBool(options, "thresholdGrayscale"));
  PipelineBaton_SetTrimThreshold(baton, sharp::AttrAsDouble(options, "trimThreshold"));
  PipelineBaton_SetGamma(baton, sharp::AttrAsDouble(options, "gamma"));
  PipelineBaton_SetGammaOut(baton, sharp::AttrAsDouble(options, "gammaOut"));
  PipelineBaton_SetLinearA(baton, sharp::AttrAsDouble(options, "linearA"));
  PipelineBaton_SetLinearB(baton, sharp::AttrAsDouble(options, "linearB"));
  PipelineBaton_SetGreyscale(baton, sharp::AttrAsBool(options, "greyscale"));
  PipelineBaton_SetNormalise(baton, sharp::AttrAsBool(options, "normalise"));
  PipelineBaton_SetClaheWidth(baton, sharp::AttrAsUint32(options, "claheWidth"));
  PipelineBaton_SetClaheHeight(baton, sharp::AttrAsUint32(options, "claheHeight"));
  PipelineBaton_SetClaheMaxSlope(baton, sharp::AttrAsUint32(options, "claheMaxSlope"));
  PipelineBaton_SetUseExifOrientation(baton, sharp::AttrAsBool(options, "useExifOrientation"));
  PipelineBaton_SetAngle(baton, sharp::AttrAsInt32(options, "angle"));
  PipelineBaton_SetRotationAngle(baton, sharp::AttrAsDouble(options, "rotationAngle"));
  PipelineBaton_SetRotationBackground(baton, sharp::AttrAsVectorOfDouble(options, "rotationBackground"));
  PipelineBaton_SetRotateBeforePreExtract(baton, sharp::AttrAsBool(options, "rotateBeforePreExtract"));
  PipelineBaton_SetFlip(baton, sharp::AttrAsBool(options, "flip"));
  PipelineBaton_SetFlop(baton, sharp::AttrAsBool(options, "flop"));
  PipelineBaton_SetExtendTop(baton, sharp::AttrAsInt32(options, "extendTop"));
  PipelineBaton_SetExtendBottom(baton, sharp::AttrAsInt32(options, "extendBottom"));
  PipelineBaton_SetExtendLeft(baton, sharp::AttrAsInt32(options, "extendLeft"));
  PipelineBaton_SetExtendRight(baton, sharp::AttrAsInt32(options, "extendRight"));
  PipelineBaton_SetExtendBackground(baton, sharp::AttrAsVectorOfDouble(options, "extendBackground"));
  PipelineBaton_SetExtractChannel(baton, sharp::AttrAsInt32(options, "extractChannel"));
  PipelineBaton_SetAffineMatrix(baton, sharp::AttrAsVectorOfDouble(options, "affineMatrix"));
  PipelineBaton_SetAffineBackground(baton, sharp::AttrAsVectorOfDouble(options, "affineBackground"));
  PipelineBaton_SetAffineIdx(baton, sharp::AttrAsDouble(options, "affineIdx"));
  PipelineBaton_SetAffineIdy(baton, sharp::AttrAsDouble(options, "affineIdy"));
  PipelineBaton_SetAffineOdx(baton, sharp::AttrAsDouble(options, "affineOdx"));
  PipelineBaton_SetAffineOdy(baton, sharp::AttrAsDouble(options, "affineOdy"));
  PipelineBaton_SetAffineInterpolator(baton, sharp::AttrAsStr(options, "affineInterpolator").c_str());
  PipelineBaton_SetRemoveAlpha(baton, sharp::AttrAsBool(options, "removeAlpha"));
  PipelineBaton_SetEnsureAlpha(baton, sharp::AttrAsDouble(options, "ensureAlpha"));
  if (options.Has("boolean")) {
    PipelineBaton_SetBoolean(baton, sharp::CreateInputDescriptor(options.Get("boolean").As<Napi::Object>()));
    PipelineBaton_SetBooleanOp(baton, sharp::GetBooleanOperation(sharp::AttrAsStr(options, "booleanOp")));
  }
  if (options.Has("bandBoolOp")) {
    PipelineBaton_SetBandBoolOp(baton, sharp::GetBooleanOperation(sharp::AttrAsStr(options, "bandBoolOp")));
  }
  if (options.Has("convKernel")) {
    Napi::Object kernel = options.Get("convKernel").As<Napi::Object>();
    PipelineBaton_SetConvKernelWidth(baton, sharp::AttrAsUint32(kernel, "width"));
    PipelineBaton_SetConvKernelHeight(baton, sharp::AttrAsUint32(kernel, "height"));
    PipelineBaton_SetConvKernelScale(baton, sharp::AttrAsDouble(kernel, "scale"));
    PipelineBaton_SetConvKernelOffset(baton, sharp::AttrAsDouble(kernel, "offset"));
    size_t const kernelSize = static_cast<size_t>(PipelineBaton_GetConvKernelWidth(baton) * PipelineBaton_GetConvKernelHeight(baton));
    PipelineBaton_SetConvKernel(baton, std::unique_ptr<double[]>(new double[kernelSize]));
    Napi::Array kdata = kernel.Get("kernel").As<Napi::Array>();
    for (unsigned int i = 0; i < kernelSize; i++) {
      PipelineBaton_GetConvKernel(baton)[i] = sharp::AttrAsDouble(kdata, i);
    }
  }
  if (options.Has("recombMatrix")) {
    PipelineBaton_SetRecombMatrix(baton, std::unique_ptr<double[]>(new double[9]));
    Napi::Array recombMatrix = options.Get("recombMatrix").As<Napi::Array>();
    for (unsigned int i = 0; i < 9; i++) {
       PipelineBaton_GetRecombMatrix(baton)[i] = sharp::AttrAsDouble(recombMatrix, i);
    }
  }
  PipelineBaton_SetColourspaceInput(baton, sharp::GetInterpretation(sharp::AttrAsStr(options, "colourspaceInput")));
  if (PipelineBaton_GetColourspaceInput(baton) == VIPS_INTERPRETATION_ERROR) {
    PipelineBaton_SetColourspaceInput(baton, VIPS_INTERPRETATION_LAST);
  }
  PipelineBaton_SetColourspace(baton, sharp::GetInterpretation(sharp::AttrAsStr(options, "colourspace")));
  if (PipelineBaton_GetColourspace(baton) == VIPS_INTERPRETATION_ERROR) {
    PipelineBaton_SetColourspace(baton, VIPS_INTERPRETATION_sRGB);
  }
  // Output
  PipelineBaton_SetFormatOut(baton, sharp::AttrAsStr(options, "formatOut").c_str());
  PipelineBaton_SetFileOut(baton, sharp::AttrAsStr(options, "fileOut").c_str());
  PipelineBaton_SetWithMetadata(baton, sharp::AttrAsBool(options, "withMetadata"));
  PipelineBaton_SetWithMetadataOrientation(baton, sharp::AttrAsUint32(options, "withMetadataOrientation"));
  PipelineBaton_SetWithMetadataDensity(baton, sharp::AttrAsDouble(options, "withMetadataDensity"));
  PipelineBaton_SetWithMetadataIcc(baton, sharp::AttrAsStr(options, "withMetadataIcc").c_str());
  Napi::Object mdStrs = options.Get("withMetadataStrs").As<Napi::Object>();
  Napi::Array mdStrKeys = mdStrs.GetPropertyNames();
  for (unsigned int i = 0; i < mdStrKeys.Length(); i++) {
    std::string k = sharp::AttrAsStr(mdStrKeys, i);
    PipelineBaton_WithMetadataStrs_Insert(baton, std::make_pair(k, sharp::AttrAsStr(mdStrs, k)));
  }
  PipelineBaton_SetTimeoutSeconds(baton, sharp::AttrAsUint32(options, "timeoutSeconds"));
  // Format-specific
  PipelineBaton_SetJpegQuality(baton, sharp::AttrAsUint32(options, "jpegQuality"));
  PipelineBaton_SetJpegProgressive(baton, sharp::AttrAsBool(options, "jpegProgressive"));
  PipelineBaton_SetJpegChromaSubsampling(baton, sharp::AttrAsStr(options, "jpegChromaSubsampling").c_str());
  PipelineBaton_SetJpegTrellisQuantisation(baton, sharp::AttrAsBool(options, "jpegTrellisQuantisation"));
  PipelineBaton_SetJpegQuantisationTable(baton, sharp::AttrAsUint32(options, "jpegQuantisationTable"));
  PipelineBaton_SetJpegOvershootDeringing(baton, sharp::AttrAsBool(options, "jpegOvershootDeringing"));
  PipelineBaton_SetJpegOptimiseScans(baton, sharp::AttrAsBool(options, "jpegOptimiseScans"));
  PipelineBaton_SetJpegOptimiseCoding(baton, sharp::AttrAsBool(options, "jpegOptimiseCoding"));
  PipelineBaton_SetPngProgressive(baton, sharp::AttrAsBool(options, "pngProgressive"));
  PipelineBaton_SetPngCompressionLevel(baton, sharp::AttrAsUint32(options, "pngCompressionLevel"));
  PipelineBaton_SetPngAdaptiveFiltering(baton, sharp::AttrAsBool(options, "pngAdaptiveFiltering"));
  PipelineBaton_SetPngPalette(baton, sharp::AttrAsBool(options, "pngPalette"));
  PipelineBaton_SetPngQuality(baton, sharp::AttrAsUint32(options, "pngQuality"));
  PipelineBaton_SetPngEffort(baton, sharp::AttrAsUint32(options, "pngEffort"));
  PipelineBaton_SetPngBitdepth(baton, sharp::AttrAsUint32(options, "pngBitdepth"));
  PipelineBaton_SetPngDither(baton, sharp::AttrAsDouble(options, "pngDither"));
  PipelineBaton_SetJp2Quality(baton, sharp::AttrAsUint32(options, "jp2Quality"));
  PipelineBaton_SetJp2Lossless(baton, sharp::AttrAsBool(options, "jp2Lossless"));
  PipelineBaton_SetJp2TileHeight(baton, sharp::AttrAsUint32(options, "jp2TileHeight"));
  PipelineBaton_SetJp2TileWidth(baton, sharp::AttrAsUint32(options, "jp2TileWidth"));
  PipelineBaton_SetJp2ChromaSubsampling(baton, sharp::AttrAsStr(options, "jp2ChromaSubsampling").c_str());
  PipelineBaton_SetWebpQuality(baton, sharp::AttrAsUint32(options, "webpQuality"));
  PipelineBaton_SetWebpAlphaQuality(baton, sharp::AttrAsUint32(options, "webpAlphaQuality"));
  PipelineBaton_SetWebpLossless(baton, sharp::AttrAsBool(options, "webpLossless"));
  PipelineBaton_SetWebpNearLossless(baton, sharp::AttrAsBool(options, "webpNearLossless"));
  PipelineBaton_SetWebpSmartSubsample(baton, sharp::AttrAsBool(options, "webpSmartSubsample"));
  PipelineBaton_SetWebpEffort(baton, sharp::AttrAsUint32(options, "webpEffort"));
  PipelineBaton_SetGifBitdepth(baton, sharp::AttrAsUint32(options, "gifBitdepth"));
  PipelineBaton_SetGifEffort(baton, sharp::AttrAsUint32(options, "gifEffort"));
  PipelineBaton_SetGifDither(baton, sharp::AttrAsDouble(options, "gifDither"));
  PipelineBaton_SetTiffQuality(baton, sharp::AttrAsUint32(options, "tiffQuality"));
  PipelineBaton_SetTiffPyramid(baton, sharp::AttrAsBool(options, "tiffPyramid"));
  PipelineBaton_SetTiffBitdepth(baton, sharp::AttrAsUint32(options, "tiffBitdepth"));
  PipelineBaton_SetTiffTile(baton, sharp::AttrAsBool(options, "tiffTile"));
  PipelineBaton_SetTiffTileWidth(baton, sharp::AttrAsUint32(options, "tiffTileWidth"));
  PipelineBaton_SetTiffTileHeight(baton, sharp::AttrAsUint32(options, "tiffTileHeight"));
  PipelineBaton_SetTiffXres(baton, sharp::AttrAsDouble(options, "tiffXres"));
  PipelineBaton_SetTiffYres(baton, sharp::AttrAsDouble(options, "tiffYres"));
  if (PipelineBaton_GetTiffXres(baton) == 1.0 && PipelineBaton_GetTiffYres(baton) == 1.0 && PipelineBaton_GetWithMetadataDensity(baton) > 0) {
    auto val = PipelineBaton_GetWithMetadataDensity(baton) / 25.4;
    PipelineBaton_SetTiffXres(baton, val);
    PipelineBaton_SetTiffYres(baton, val);
  }
  // tiff compression options
  PipelineBaton_SetTiffCompression(baton, static_cast<VipsForeignTiffCompression>(
  vips_enum_from_nick(nullptr, VIPS_TYPE_FOREIGN_TIFF_COMPRESSION,
    sharp::AttrAsStr(options, "tiffCompression").data())));
  PipelineBaton_SetTiffPredictor(baton, static_cast<VipsForeignTiffPredictor>(
  vips_enum_from_nick(nullptr, VIPS_TYPE_FOREIGN_TIFF_PREDICTOR,
    sharp::AttrAsStr(options, "tiffPredictor").data())));
  PipelineBaton_SetTiffResolutionUnit(baton, static_cast<VipsForeignTiffResunit>(
  vips_enum_from_nick(nullptr, VIPS_TYPE_FOREIGN_TIFF_RESUNIT,
    sharp::AttrAsStr(options, "tiffResolutionUnit").data())));

  PipelineBaton_SetHeifQuality(baton, sharp::AttrAsUint32(options, "heifQuality"));
  PipelineBaton_SetHeifLossless(baton, sharp::AttrAsBool(options, "heifLossless"));
  PipelineBaton_SetHeifCompression(baton, static_cast<VipsForeignHeifCompression>(
    vips_enum_from_nick(nullptr, VIPS_TYPE_FOREIGN_HEIF_COMPRESSION,
    sharp::AttrAsStr(options, "heifCompression").data())));
  PipelineBaton_SetHeifEffort(baton, sharp::AttrAsUint32(options, "heifEffort"));
  PipelineBaton_SetHeifChromaSubsampling(baton, sharp::AttrAsStr(options, "heifChromaSubsampling").c_str());
  // Raw output
  PipelineBaton_SetRawDepth(baton, static_cast<VipsBandFormat>(
    vips_enum_from_nick(nullptr, VIPS_TYPE_BAND_FORMAT,
    sharp::AttrAsStr(options, "rawDepth").data())));
  // Animated output properties
  if (sharp::HasAttr(options, "loop")) {
    PipelineBaton_SetLoop(baton, sharp::AttrAsUint32(options, "loop"));
  }
  if (sharp::HasAttr(options, "delay")) {
    PipelineBaton_SetDelay(baton, sharp::AttrAsInt32Vector(options, "delay"));
  }
  // Tile output
  PipelineBaton_SetTileSize(baton, sharp::AttrAsUint32(options, "tileSize"));
  PipelineBaton_SetTileOverlap(baton, sharp::AttrAsUint32(options, "tileOverlap"));
  PipelineBaton_SetTileAngle(baton, sharp::AttrAsInt32(options, "tileAngle"));
  PipelineBaton_SetTileBackground(baton, sharp::AttrAsVectorOfDouble(options, "tileBackground"));
  PipelineBaton_SetTileSkipBlanks(baton, sharp::AttrAsInt32(options, "tileSkipBlanks"));
  PipelineBaton_SetTileContainer(baton, static_cast<VipsForeignDzContainer>(
    vips_enum_from_nick(nullptr, VIPS_TYPE_FOREIGN_DZ_CONTAINER,
    sharp::AttrAsStr(options, "tileContainer").data())));
  PipelineBaton_SetTileLayout(baton, static_cast<VipsForeignDzLayout>(
    vips_enum_from_nick(nullptr, VIPS_TYPE_FOREIGN_DZ_LAYOUT,
    sharp::AttrAsStr(options, "tileLayout").data())));
  PipelineBaton_SetTileFormat(baton, sharp::AttrAsStr(options, "tileFormat").c_str());
  PipelineBaton_SetTileDepth(baton, static_cast<VipsForeignDzDepth>(
    vips_enum_from_nick(nullptr, VIPS_TYPE_FOREIGN_DZ_DEPTH,
    sharp::AttrAsStr(options, "tileDepth").data())));
  PipelineBaton_SetTileCentre(baton, sharp::AttrAsBool(options, "tileCentre"));
  PipelineBaton_SetTileId(baton, sharp::AttrAsStr(options, "tileId").c_str());

  // Force random access for certain operations
  sharp::InputDescriptor* input = PipelineBaton_GetInput(baton);
  if (InputDescriptor_GetAccess(input) == VIPS_ACCESS_SEQUENTIAL) {
    if (
      PipelineBaton_GetTrimThreshold(baton) > 0.0 ||
      PipelineBaton_GetNormalise(baton) ||
      PipelineBaton_GetPosition(baton) == 16 || PipelineBaton_GetPosition(baton) == 17 ||
      PipelineBaton_GetAngle(baton) % 360 != 0 ||
      fmod(PipelineBaton_GetRotationAngle(baton), 360.0) != 0.0 ||
      PipelineBaton_GetUseExifOrientation(baton)
    ) {
      InputDescriptor_SetAccess(input, VIPS_ACCESS_RANDOM);
    }
  }

  // Function to notify of libvips warnings
  Napi::Function debuglog = options.Get("debuglog").As<Napi::Function>();

  // Function to notify of queue length changes
  Napi::Function queueListener = options.Get("queueListener").As<Napi::Function>();

  // Join queue for worker thread
  Napi::Function callback = info[1].As<Napi::Function>();
  PipelineWorker *worker = new PipelineWorker(callback, baton, debuglog, queueListener);
  worker->Receiver().Set("options", options);
  worker->Queue();

  // Increment queued task counter
  g_atomic_int_inc(&sharp::counterQueue);
  Napi::Number queueLength = Napi::Number::New(info.Env(), static_cast<double>(sharp::counterQueue));
  queueListener.Call(info.This(), { queueLength });

  return info.Env().Undefined();
}
