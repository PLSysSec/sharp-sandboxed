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

#include "common.h"
#include "common_host.h"
#include "operations.h"
#include "pipeline.h"

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

extern "C" {
void PipelineWorkerExecute(PipelineBaton *baton);
}

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

    if (baton->err.empty()) {
      int width = baton->width;
      int height = baton->height;
      if (baton->topOffsetPre != -1 && (baton->width == -1 || baton->height == -1)) {
        width = baton->widthPre;
        height = baton->heightPre;
      }
      if (baton->topOffsetPost != -1) {
        width = baton->widthPost;
        height = baton->heightPost;
      }
      // Info Object
      Napi::Object info = Napi::Object::New(env);
      info.Set("format", baton->formatOut);
      info.Set("width", static_cast<uint32_t>(width));
      info.Set("height", static_cast<uint32_t>(height));
      info.Set("channels", static_cast<uint32_t>(baton->channels));
      if (baton->formatOut == "raw") {
        info.Set("depth", vips_enum_nick(VIPS_TYPE_BAND_FORMAT, baton->rawDepth));
      }
      info.Set("premultiplied", baton->premultiplied);
      if (baton->hasCropOffset) {
        info.Set("cropOffsetLeft", static_cast<int32_t>(baton->cropOffsetLeft));
        info.Set("cropOffsetTop", static_cast<int32_t>(baton->cropOffsetTop));
      }
      if (baton->trimThreshold > 0.0) {
        info.Set("trimOffsetLeft", static_cast<int32_t>(baton->trimOffsetLeft));
        info.Set("trimOffsetTop", static_cast<int32_t>(baton->trimOffsetTop));
      }

      if (baton->bufferOutLength > 0) {
        // Add buffer size to info
        info.Set("size", static_cast<uint32_t>(baton->bufferOutLength));
        // Pass ownership of output data to Buffer instance
        Napi::Buffer<char> data = Napi::Buffer<char>::New(env, static_cast<char*>(baton->bufferOut),
          baton->bufferOutLength, sharp::FreeCallback);
        Callback().MakeCallback(Receiver().Value(), { env.Null(), data, info });
      } else {
        // Add file size to info
        struct STAT64_STRUCT st;
        if (STAT64_FUNCTION(baton->fileOut.data(), &st) == 0) {
          info.Set("size", static_cast<uint32_t>(st.st_size));
        }
        Callback().MakeCallback(Receiver().Value(), { env.Null(), info });
      }
    } else {
      Callback().MakeCallback(Receiver().Value(), { Napi::Error::New(env, baton->err).Value() });
    }

    // Delete baton
    delete baton->input;
    delete baton->boolean;
    for (Composite *composite : baton->composite) {
      delete composite->input;
      delete composite;
    }
    for (sharp::InputDescriptor *input : baton->joinChannelIn) {
      delete input;
    }
    delete baton;

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
  PipelineBaton *baton = new PipelineBaton;
  Napi::Object options = info[0].As<Napi::Object>();

  // Input
  baton->input = sharp::CreateInputDescriptor(options.Get("input").As<Napi::Object>());
  // Extract image options
  baton->topOffsetPre = sharp::AttrAsInt32(options, "topOffsetPre");
  baton->leftOffsetPre = sharp::AttrAsInt32(options, "leftOffsetPre");
  baton->widthPre = sharp::AttrAsInt32(options, "widthPre");
  baton->heightPre = sharp::AttrAsInt32(options, "heightPre");
  baton->topOffsetPost = sharp::AttrAsInt32(options, "topOffsetPost");
  baton->leftOffsetPost = sharp::AttrAsInt32(options, "leftOffsetPost");
  baton->widthPost = sharp::AttrAsInt32(options, "widthPost");
  baton->heightPost = sharp::AttrAsInt32(options, "heightPost");
  // Output image dimensions
  baton->width = sharp::AttrAsInt32(options, "width");
  baton->height = sharp::AttrAsInt32(options, "height");
  // Canvas option
  std::string canvas = sharp::AttrAsStr(options, "canvas");
  if (canvas == "crop") {
    baton->canvas = sharp::Canvas::CROP;
  } else if (canvas == "embed") {
    baton->canvas = sharp::Canvas::EMBED;
  } else if (canvas == "max") {
    baton->canvas = sharp::Canvas::MAX;
  } else if (canvas == "min") {
    baton->canvas = sharp::Canvas::MIN;
  } else if (canvas == "ignore_aspect") {
    baton->canvas = sharp::Canvas::IGNORE_ASPECT;
  }
  // Tint chroma
  baton->tintA = sharp::AttrAsDouble(options, "tintA");
  baton->tintB = sharp::AttrAsDouble(options, "tintB");
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
    baton->composite.push_back(composite);
  }
  // Resize options
  baton->withoutEnlargement = sharp::AttrAsBool(options, "withoutEnlargement");
  baton->withoutReduction = sharp::AttrAsBool(options, "withoutReduction");
  baton->position = sharp::AttrAsInt32(options, "position");
  baton->resizeBackground = sharp::AttrAsVectorOfDouble(options, "resizeBackground");
  baton->kernel = sharp::AttrAsStr(options, "kernel");
  baton->fastShrinkOnLoad = sharp::AttrAsBool(options, "fastShrinkOnLoad");
  // Join Channel Options
  if (options.Has("joinChannelIn")) {
    Napi::Array joinChannelArray = options.Get("joinChannelIn").As<Napi::Array>();
    for (unsigned int i = 0; i < joinChannelArray.Length(); i++) {
      baton->joinChannelIn.push_back(
        sharp::CreateInputDescriptor(joinChannelArray.Get(i).As<Napi::Object>()));
    }
  }
  // Operators
  baton->flatten = sharp::AttrAsBool(options, "flatten");
  baton->flattenBackground = sharp::AttrAsVectorOfDouble(options, "flattenBackground");
  baton->negate = sharp::AttrAsBool(options, "negate");
  baton->negateAlpha = sharp::AttrAsBool(options, "negateAlpha");
  baton->blurSigma = sharp::AttrAsDouble(options, "blurSigma");
  baton->brightness = sharp::AttrAsDouble(options, "brightness");
  baton->saturation = sharp::AttrAsDouble(options, "saturation");
  baton->hue = sharp::AttrAsInt32(options, "hue");
  baton->lightness = sharp::AttrAsDouble(options, "lightness");
  baton->medianSize = sharp::AttrAsUint32(options, "medianSize");
  baton->sharpenSigma = sharp::AttrAsDouble(options, "sharpenSigma");
  baton->sharpenM1 = sharp::AttrAsDouble(options, "sharpenM1");
  baton->sharpenM2 = sharp::AttrAsDouble(options, "sharpenM2");
  baton->sharpenX1 = sharp::AttrAsDouble(options, "sharpenX1");
  baton->sharpenY2 = sharp::AttrAsDouble(options, "sharpenY2");
  baton->sharpenY3 = sharp::AttrAsDouble(options, "sharpenY3");
  baton->threshold = sharp::AttrAsInt32(options, "threshold");
  baton->thresholdGrayscale = sharp::AttrAsBool(options, "thresholdGrayscale");
  baton->trimThreshold = sharp::AttrAsDouble(options, "trimThreshold");
  baton->gamma = sharp::AttrAsDouble(options, "gamma");
  baton->gammaOut = sharp::AttrAsDouble(options, "gammaOut");
  baton->linearA = sharp::AttrAsDouble(options, "linearA");
  baton->linearB = sharp::AttrAsDouble(options, "linearB");
  baton->greyscale = sharp::AttrAsBool(options, "greyscale");
  baton->normalise = sharp::AttrAsBool(options, "normalise");
  baton->claheWidth = sharp::AttrAsUint32(options, "claheWidth");
  baton->claheHeight = sharp::AttrAsUint32(options, "claheHeight");
  baton->claheMaxSlope = sharp::AttrAsUint32(options, "claheMaxSlope");
  baton->useExifOrientation = sharp::AttrAsBool(options, "useExifOrientation");
  baton->angle = sharp::AttrAsInt32(options, "angle");
  baton->rotationAngle = sharp::AttrAsDouble(options, "rotationAngle");
  baton->rotationBackground = sharp::AttrAsVectorOfDouble(options, "rotationBackground");
  baton->rotateBeforePreExtract = sharp::AttrAsBool(options, "rotateBeforePreExtract");
  baton->flip = sharp::AttrAsBool(options, "flip");
  baton->flop = sharp::AttrAsBool(options, "flop");
  baton->extendTop = sharp::AttrAsInt32(options, "extendTop");
  baton->extendBottom = sharp::AttrAsInt32(options, "extendBottom");
  baton->extendLeft = sharp::AttrAsInt32(options, "extendLeft");
  baton->extendRight = sharp::AttrAsInt32(options, "extendRight");
  baton->extendBackground = sharp::AttrAsVectorOfDouble(options, "extendBackground");
  baton->extractChannel = sharp::AttrAsInt32(options, "extractChannel");
  baton->affineMatrix = sharp::AttrAsVectorOfDouble(options, "affineMatrix");
  baton->affineBackground = sharp::AttrAsVectorOfDouble(options, "affineBackground");
  baton->affineIdx = sharp::AttrAsDouble(options, "affineIdx");
  baton->affineIdy = sharp::AttrAsDouble(options, "affineIdy");
  baton->affineOdx = sharp::AttrAsDouble(options, "affineOdx");
  baton->affineOdy = sharp::AttrAsDouble(options, "affineOdy");
  baton->affineInterpolator = sharp::AttrAsStr(options, "affineInterpolator");
  baton->removeAlpha = sharp::AttrAsBool(options, "removeAlpha");
  baton->ensureAlpha = sharp::AttrAsDouble(options, "ensureAlpha");
  if (options.Has("boolean")) {
    baton->boolean = sharp::CreateInputDescriptor(options.Get("boolean").As<Napi::Object>());
    baton->booleanOp = sharp::GetBooleanOperation(sharp::AttrAsStr(options, "booleanOp"));
  }
  if (options.Has("bandBoolOp")) {
    baton->bandBoolOp = sharp::GetBooleanOperation(sharp::AttrAsStr(options, "bandBoolOp"));
  }
  if (options.Has("convKernel")) {
    Napi::Object kernel = options.Get("convKernel").As<Napi::Object>();
    baton->convKernelWidth = sharp::AttrAsUint32(kernel, "width");
    baton->convKernelHeight = sharp::AttrAsUint32(kernel, "height");
    baton->convKernelScale = sharp::AttrAsDouble(kernel, "scale");
    baton->convKernelOffset = sharp::AttrAsDouble(kernel, "offset");
    size_t const kernelSize = static_cast<size_t>(baton->convKernelWidth * baton->convKernelHeight);
    baton->convKernel = std::unique_ptr<double[]>(new double[kernelSize]);
    Napi::Array kdata = kernel.Get("kernel").As<Napi::Array>();
    for (unsigned int i = 0; i < kernelSize; i++) {
      baton->convKernel[i] = sharp::AttrAsDouble(kdata, i);
    }
  }
  if (options.Has("recombMatrix")) {
    baton->recombMatrix = std::unique_ptr<double[]>(new double[9]);
    Napi::Array recombMatrix = options.Get("recombMatrix").As<Napi::Array>();
    for (unsigned int i = 0; i < 9; i++) {
       baton->recombMatrix[i] = sharp::AttrAsDouble(recombMatrix, i);
    }
  }
  baton->colourspaceInput = sharp::GetInterpretation(sharp::AttrAsStr(options, "colourspaceInput"));
  if (baton->colourspaceInput == VIPS_INTERPRETATION_ERROR) {
    baton->colourspaceInput = VIPS_INTERPRETATION_LAST;
  }
  baton->colourspace = sharp::GetInterpretation(sharp::AttrAsStr(options, "colourspace"));
  if (baton->colourspace == VIPS_INTERPRETATION_ERROR) {
    baton->colourspace = VIPS_INTERPRETATION_sRGB;
  }
  // Output
  baton->formatOut = sharp::AttrAsStr(options, "formatOut");
  baton->fileOut = sharp::AttrAsStr(options, "fileOut");
  baton->withMetadata = sharp::AttrAsBool(options, "withMetadata");
  baton->withMetadataOrientation = sharp::AttrAsUint32(options, "withMetadataOrientation");
  baton->withMetadataDensity = sharp::AttrAsDouble(options, "withMetadataDensity");
  baton->withMetadataIcc = sharp::AttrAsStr(options, "withMetadataIcc");
  Napi::Object mdStrs = options.Get("withMetadataStrs").As<Napi::Object>();
  Napi::Array mdStrKeys = mdStrs.GetPropertyNames();
  for (unsigned int i = 0; i < mdStrKeys.Length(); i++) {
    std::string k = sharp::AttrAsStr(mdStrKeys, i);
    baton->withMetadataStrs.insert(std::make_pair(k, sharp::AttrAsStr(mdStrs, k)));
  }
  baton->timeoutSeconds = sharp::AttrAsUint32(options, "timeoutSeconds");
  // Format-specific
  baton->jpegQuality = sharp::AttrAsUint32(options, "jpegQuality");
  baton->jpegProgressive = sharp::AttrAsBool(options, "jpegProgressive");
  baton->jpegChromaSubsampling = sharp::AttrAsStr(options, "jpegChromaSubsampling");
  baton->jpegTrellisQuantisation = sharp::AttrAsBool(options, "jpegTrellisQuantisation");
  baton->jpegQuantisationTable = sharp::AttrAsUint32(options, "jpegQuantisationTable");
  baton->jpegOvershootDeringing = sharp::AttrAsBool(options, "jpegOvershootDeringing");
  baton->jpegOptimiseScans = sharp::AttrAsBool(options, "jpegOptimiseScans");
  baton->jpegOptimiseCoding = sharp::AttrAsBool(options, "jpegOptimiseCoding");
  baton->pngProgressive = sharp::AttrAsBool(options, "pngProgressive");
  baton->pngCompressionLevel = sharp::AttrAsUint32(options, "pngCompressionLevel");
  baton->pngAdaptiveFiltering = sharp::AttrAsBool(options, "pngAdaptiveFiltering");
  baton->pngPalette = sharp::AttrAsBool(options, "pngPalette");
  baton->pngQuality = sharp::AttrAsUint32(options, "pngQuality");
  baton->pngEffort = sharp::AttrAsUint32(options, "pngEffort");
  baton->pngBitdepth = sharp::AttrAsUint32(options, "pngBitdepth");
  baton->pngDither = sharp::AttrAsDouble(options, "pngDither");
  baton->jp2Quality = sharp::AttrAsUint32(options, "jp2Quality");
  baton->jp2Lossless = sharp::AttrAsBool(options, "jp2Lossless");
  baton->jp2TileHeight = sharp::AttrAsUint32(options, "jp2TileHeight");
  baton->jp2TileWidth = sharp::AttrAsUint32(options, "jp2TileWidth");
  baton->jp2ChromaSubsampling = sharp::AttrAsStr(options, "jp2ChromaSubsampling");
  baton->webpQuality = sharp::AttrAsUint32(options, "webpQuality");
  baton->webpAlphaQuality = sharp::AttrAsUint32(options, "webpAlphaQuality");
  baton->webpLossless = sharp::AttrAsBool(options, "webpLossless");
  baton->webpNearLossless = sharp::AttrAsBool(options, "webpNearLossless");
  baton->webpSmartSubsample = sharp::AttrAsBool(options, "webpSmartSubsample");
  baton->webpEffort = sharp::AttrAsUint32(options, "webpEffort");
  baton->gifBitdepth = sharp::AttrAsUint32(options, "gifBitdepth");
  baton->gifEffort = sharp::AttrAsUint32(options, "gifEffort");
  baton->gifDither = sharp::AttrAsDouble(options, "gifDither");
  baton->tiffQuality = sharp::AttrAsUint32(options, "tiffQuality");
  baton->tiffPyramid = sharp::AttrAsBool(options, "tiffPyramid");
  baton->tiffBitdepth = sharp::AttrAsUint32(options, "tiffBitdepth");
  baton->tiffTile = sharp::AttrAsBool(options, "tiffTile");
  baton->tiffTileWidth = sharp::AttrAsUint32(options, "tiffTileWidth");
  baton->tiffTileHeight = sharp::AttrAsUint32(options, "tiffTileHeight");
  baton->tiffXres = sharp::AttrAsDouble(options, "tiffXres");
  baton->tiffYres = sharp::AttrAsDouble(options, "tiffYres");
  if (baton->tiffXres == 1.0 && baton->tiffYres == 1.0 && baton->withMetadataDensity > 0) {
    baton->tiffXres = baton->tiffYres = baton->withMetadataDensity / 25.4;
  }
  // tiff compression options
  baton->tiffCompression = static_cast<VipsForeignTiffCompression>(
  vips_enum_from_nick(nullptr, VIPS_TYPE_FOREIGN_TIFF_COMPRESSION,
    sharp::AttrAsStr(options, "tiffCompression").data()));
  baton->tiffPredictor = static_cast<VipsForeignTiffPredictor>(
  vips_enum_from_nick(nullptr, VIPS_TYPE_FOREIGN_TIFF_PREDICTOR,
    sharp::AttrAsStr(options, "tiffPredictor").data()));
  baton->tiffResolutionUnit = static_cast<VipsForeignTiffResunit>(
  vips_enum_from_nick(nullptr, VIPS_TYPE_FOREIGN_TIFF_RESUNIT,
    sharp::AttrAsStr(options, "tiffResolutionUnit").data()));

  baton->heifQuality = sharp::AttrAsUint32(options, "heifQuality");
  baton->heifLossless = sharp::AttrAsBool(options, "heifLossless");
  baton->heifCompression = static_cast<VipsForeignHeifCompression>(
    vips_enum_from_nick(nullptr, VIPS_TYPE_FOREIGN_HEIF_COMPRESSION,
    sharp::AttrAsStr(options, "heifCompression").data()));
  baton->heifEffort = sharp::AttrAsUint32(options, "heifEffort");
  baton->heifChromaSubsampling = sharp::AttrAsStr(options, "heifChromaSubsampling");
  // Raw output
  baton->rawDepth = static_cast<VipsBandFormat>(
    vips_enum_from_nick(nullptr, VIPS_TYPE_BAND_FORMAT,
    sharp::AttrAsStr(options, "rawDepth").data()));
  // Animated output properties
  if (sharp::HasAttr(options, "loop")) {
    baton->loop = sharp::AttrAsUint32(options, "loop");
  }
  if (sharp::HasAttr(options, "delay")) {
    baton->delay = sharp::AttrAsInt32Vector(options, "delay");
  }
  // Tile output
  baton->tileSize = sharp::AttrAsUint32(options, "tileSize");
  baton->tileOverlap = sharp::AttrAsUint32(options, "tileOverlap");
  baton->tileAngle = sharp::AttrAsInt32(options, "tileAngle");
  baton->tileBackground = sharp::AttrAsVectorOfDouble(options, "tileBackground");
  baton->tileSkipBlanks = sharp::AttrAsInt32(options, "tileSkipBlanks");
  baton->tileContainer = static_cast<VipsForeignDzContainer>(
    vips_enum_from_nick(nullptr, VIPS_TYPE_FOREIGN_DZ_CONTAINER,
    sharp::AttrAsStr(options, "tileContainer").data()));
  baton->tileLayout = static_cast<VipsForeignDzLayout>(
    vips_enum_from_nick(nullptr, VIPS_TYPE_FOREIGN_DZ_LAYOUT,
    sharp::AttrAsStr(options, "tileLayout").data()));
  baton->tileFormat = sharp::AttrAsStr(options, "tileFormat");
  baton->tileDepth = static_cast<VipsForeignDzDepth>(
    vips_enum_from_nick(nullptr, VIPS_TYPE_FOREIGN_DZ_DEPTH,
    sharp::AttrAsStr(options, "tileDepth").data()));
  baton->tileCentre = sharp::AttrAsBool(options, "tileCentre");
  baton->tileId = sharp::AttrAsStr(options, "tileId");

  // Force random access for certain operations
  if (baton->input->access == VIPS_ACCESS_SEQUENTIAL) {
    if (
      baton->trimThreshold > 0.0 ||
      baton->normalise ||
      baton->position == 16 || baton->position == 17 ||
      baton->angle % 360 != 0 ||
      fmod(baton->rotationAngle, 360.0) != 0.0 ||
      baton->useExifOrientation
    ) {
      baton->input->access = VIPS_ACCESS_RANDOM;
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
