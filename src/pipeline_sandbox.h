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

#ifndef SRC_PIPELINE_SANDBOX_H_
#define SRC_PIPELINE_SANDBOX_H_

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>

#include <vips/vips8>

#include "canvas.h"

namespace sharp {
  struct InputDescriptor;
}
struct Composite {
  InputDescriptor *input;
  VipsBlendMode mode;
  int gravity;
  int left;
  int top;
  bool hasOffset;
  bool tile;
  bool premultiplied;

  Composite():
    input(nullptr),
    mode(VIPS_BLEND_MODE_OVER),
    gravity(0),
    left(0),
    top(0),
    hasOffset(false),
    tile(false),
    premultiplied(false) {}
};

struct PipelineBaton {
  InputDescriptor *input;
  std::string formatOut;
  std::string fileOut;
  void *bufferOut;
  size_t bufferOutLength;
  std::vector<Composite *> composite;
  std::vector<InputDescriptor *> joinChannelIn;
  int topOffsetPre;
  int leftOffsetPre;
  int widthPre;
  int heightPre;
  int topOffsetPost;
  int leftOffsetPost;
  int widthPost;
  int heightPost;
  int width;
  int height;
  int channels;
  sharp::Canvas canvas;
  int position;
  std::vector<double> resizeBackground;
  bool hasCropOffset;
  int cropOffsetLeft;
  int cropOffsetTop;
  bool premultiplied;
  bool tileCentre;
  std::string kernel;
  bool fastShrinkOnLoad;
  double tintA;
  double tintB;
  bool flatten;
  std::vector<double> flattenBackground;
  bool negate;
  bool negateAlpha;
  double blurSigma;
  double brightness;
  double saturation;
  int hue;
  double lightness;
  int medianSize;
  double sharpenSigma;
  double sharpenM1;
  double sharpenM2;
  double sharpenX1;
  double sharpenY2;
  double sharpenY3;
  int threshold;
  bool thresholdGrayscale;
  double trimThreshold;
  int trimOffsetLeft;
  int trimOffsetTop;
  double linearA;
  double linearB;
  double gamma;
  double gammaOut;
  bool greyscale;
  bool normalise;
  int claheWidth;
  int claheHeight;
  int claheMaxSlope;
  bool useExifOrientation;
  int angle;
  double rotationAngle;
  std::vector<double> rotationBackground;
  bool rotateBeforePreExtract;
  bool flip;
  bool flop;
  int extendTop;
  int extendBottom;
  int extendLeft;
  int extendRight;
  std::vector<double> extendBackground;
  bool withoutEnlargement;
  bool withoutReduction;
  std::vector<double> affineMatrix;
  std::vector<double> affineBackground;
  double affineIdx;
  double affineIdy;
  double affineOdx;
  double affineOdy;
  std::string affineInterpolator;
  int jpegQuality;
  bool jpegProgressive;
  std::string jpegChromaSubsampling;
  bool jpegTrellisQuantisation;
  int jpegQuantisationTable;
  bool jpegOvershootDeringing;
  bool jpegOptimiseScans;
  bool jpegOptimiseCoding;
  bool pngProgressive;
  int pngCompressionLevel;
  bool pngAdaptiveFiltering;
  bool pngPalette;
  int pngQuality;
  int pngEffort;
  int pngBitdepth;
  double pngDither;
  int jp2Quality;
  bool jp2Lossless;
  int jp2TileHeight;
  int jp2TileWidth;
  std::string jp2ChromaSubsampling;
  int webpQuality;
  int webpAlphaQuality;
  bool webpNearLossless;
  bool webpLossless;
  bool webpSmartSubsample;
  int webpEffort;
  int gifBitdepth;
  int gifEffort;
  double gifDither;
  int tiffQuality;
  VipsForeignTiffCompression tiffCompression;
  VipsForeignTiffPredictor tiffPredictor;
  bool tiffPyramid;
  int tiffBitdepth;
  bool tiffTile;
  int tiffTileHeight;
  int tiffTileWidth;
  double tiffXres;
  double tiffYres;
  VipsForeignTiffResunit tiffResolutionUnit;
  int heifQuality;
  VipsForeignHeifCompression heifCompression;
  int heifEffort;
  std::string heifChromaSubsampling;
  bool heifLossless;
  VipsBandFormat rawDepth;
  std::string err;
  bool withMetadata;
  int withMetadataOrientation;
  double withMetadataDensity;
  std::string withMetadataIcc;
  std::unordered_map<std::string, std::string> withMetadataStrs;
  int timeoutSeconds;
  std::unique_ptr<double[]> convKernel;
  int convKernelWidth;
  int convKernelHeight;
  double convKernelScale;
  double convKernelOffset;
  InputDescriptor *boolean;
  VipsOperationBoolean booleanOp;
  VipsOperationBoolean bandBoolOp;
  int extractChannel;
  bool removeAlpha;
  double ensureAlpha;
  VipsInterpretation colourspaceInput;
  VipsInterpretation colourspace;
  std::vector<int> delay;
  int loop;
  int tileSize;
  int tileOverlap;
  VipsForeignDzContainer tileContainer;
  VipsForeignDzLayout tileLayout;
  std::string tileFormat;
  int tileAngle;
  std::vector<double> tileBackground;
  int tileSkipBlanks;
  VipsForeignDzDepth tileDepth;
  std::string tileId;
  std::unique_ptr<double[]> recombMatrix;

  PipelineBaton():
    input(nullptr),
    bufferOutLength(0),
    topOffsetPre(-1),
    topOffsetPost(-1),
    channels(0),
    canvas(sharp::Canvas::CROP),
    position(0),
    resizeBackground{ 0.0, 0.0, 0.0, 255.0 },
    hasCropOffset(false),
    cropOffsetLeft(0),
    cropOffsetTop(0),
    premultiplied(false),
    tintA(128.0),
    tintB(128.0),
    flatten(false),
    flattenBackground{ 0.0, 0.0, 0.0 },
    negate(false),
    negateAlpha(true),
    blurSigma(0.0),
    brightness(1.0),
    saturation(1.0),
    hue(0),
    lightness(0),
    medianSize(0),
    sharpenSigma(0.0),
    sharpenM1(1.0),
    sharpenM2(2.0),
    sharpenX1(2.0),
    sharpenY2(10.0),
    sharpenY3(20.0),
    threshold(0),
    thresholdGrayscale(true),
    trimThreshold(0.0),
    trimOffsetLeft(0),
    trimOffsetTop(0),
    linearA(1.0),
    linearB(0.0),
    gamma(0.0),
    greyscale(false),
    normalise(false),
    claheWidth(0),
    claheHeight(0),
    claheMaxSlope(3),
    useExifOrientation(false),
    angle(0),
    rotationAngle(0.0),
    rotationBackground{ 0.0, 0.0, 0.0, 255.0 },
    flip(false),
    flop(false),
    extendTop(0),
    extendBottom(0),
    extendLeft(0),
    extendRight(0),
    extendBackground{ 0.0, 0.0, 0.0, 255.0 },
    withoutEnlargement(false),
    withoutReduction(false),
    affineMatrix{ 1.0, 0.0, 0.0, 1.0 },
    affineBackground{ 0.0, 0.0, 0.0, 255.0 },
    affineIdx(0),
    affineIdy(0),
    affineOdx(0),
    affineOdy(0),
    affineInterpolator("bicubic"),
    jpegQuality(80),
    jpegProgressive(false),
    jpegChromaSubsampling("4:2:0"),
    jpegTrellisQuantisation(false),
    jpegQuantisationTable(0),
    jpegOvershootDeringing(false),
    jpegOptimiseScans(false),
    jpegOptimiseCoding(true),
    pngProgressive(false),
    pngCompressionLevel(6),
    pngAdaptiveFiltering(false),
    pngPalette(false),
    pngQuality(100),
    pngEffort(7),
    pngBitdepth(8),
    pngDither(1.0),
    jp2Quality(80),
    jp2Lossless(false),
    jp2TileHeight(512),
    jp2TileWidth(512),
    jp2ChromaSubsampling("4:4:4"),
    webpQuality(80),
    webpAlphaQuality(100),
    webpNearLossless(false),
    webpLossless(false),
    webpSmartSubsample(false),
    webpEffort(4),
    tiffQuality(80),
    tiffCompression(VIPS_FOREIGN_TIFF_COMPRESSION_JPEG),
    tiffPredictor(VIPS_FOREIGN_TIFF_PREDICTOR_HORIZONTAL),
    tiffPyramid(false),
    tiffBitdepth(8),
    tiffTile(false),
    tiffTileHeight(256),
    tiffTileWidth(256),
    tiffXres(1.0),
    tiffYres(1.0),
    tiffResolutionUnit(VIPS_FOREIGN_TIFF_RESUNIT_INCH),
    heifQuality(50),
    heifCompression(VIPS_FOREIGN_HEIF_COMPRESSION_AV1),
    heifEffort(4),
    heifChromaSubsampling("4:4:4"),
    heifLossless(false),
    rawDepth(VIPS_FORMAT_UCHAR),
    withMetadata(false),
    withMetadataOrientation(-1),
    withMetadataDensity(0.0),
    timeoutSeconds(0),
    convKernelWidth(0),
    convKernelHeight(0),
    convKernelScale(0.0),
    convKernelOffset(0.0),
    boolean(nullptr),
    booleanOp(VIPS_OPERATION_BOOLEAN_LAST),
    bandBoolOp(VIPS_OPERATION_BOOLEAN_LAST),
    extractChannel(-1),
    removeAlpha(false),
    ensureAlpha(-1.0),
    colourspaceInput(VIPS_INTERPRETATION_LAST),
    colourspace(VIPS_INTERPRETATION_LAST),
    loop(-1),
    tileSize(256),
    tileOverlap(0),
    tileContainer(VIPS_FOREIGN_DZ_CONTAINER_FS),
    tileLayout(VIPS_FOREIGN_DZ_LAYOUT_DZ),
    tileAngle(0),
    tileBackground{ 255.0, 255.0, 255.0, 255.0 },
    tileSkipBlanks(-1),
    tileDepth(VIPS_FOREIGN_DZ_DEPTH_LAST) {}
};

extern "C" {
  void PipelineWorkerExecute(PipelineBaton* baton);

  PipelineBaton* CreatePipelineBaton();
  void DestroyPipelineBaton(PipelineBaton* baton);

  InputDescriptor* PipelineBaton_GetInput(PipelineBaton* baton);
  void PipelineBaton_SetInput(PipelineBaton* baton, InputDescriptor* val);
  const char* PipelineBaton_GetFormatOut(PipelineBaton* baton);
  void PipelineBaton_SetFormatOut(PipelineBaton* baton, const char* val);
  const char* PipelineBaton_GetFileOut(PipelineBaton* baton);
  void PipelineBaton_SetFileOut(PipelineBaton* baton, const char* val);
  void* PipelineBaton_GetBufferOut(PipelineBaton* baton);
  void PipelineBaton_SetBufferOut(PipelineBaton* baton, void* val);
  size_t PipelineBaton_GetBufferOutLength(PipelineBaton* baton);
  void PipelineBaton_SetBufferOutLength(PipelineBaton* baton, size_t val);
  Composite ** PipelineBaton_GetComposite(PipelineBaton* baton);
  void PipelineBaton_SetComposite(PipelineBaton* baton, std::vector<Composite *> val);
  InputDescriptor ** PipelineBaton_GetJoinChannelIn(PipelineBaton* baton);
  void PipelineBaton_SetJoinChannelIn(PipelineBaton* baton, std::vector<InputDescriptor *> val);
  int PipelineBaton_GetTopOffsetPre(PipelineBaton* baton);
  void PipelineBaton_SetTopOffsetPre(PipelineBaton* baton, int val);
  int PipelineBaton_GetLeftOffsetPre(PipelineBaton* baton);
  void PipelineBaton_SetLeftOffsetPre(PipelineBaton* baton, int val);
  int PipelineBaton_GetWidthPre(PipelineBaton* baton);
  void PipelineBaton_SetWidthPre(PipelineBaton* baton, int val);
  int PipelineBaton_GetHeightPre(PipelineBaton* baton);
  void PipelineBaton_SetHeightPre(PipelineBaton* baton, int val);
  int PipelineBaton_GetTopOffsetPost(PipelineBaton* baton);
  void PipelineBaton_SetTopOffsetPost(PipelineBaton* baton, int val);
  int PipelineBaton_GetLeftOffsetPost(PipelineBaton* baton);
  void PipelineBaton_SetLeftOffsetPost(PipelineBaton* baton, int val);
  int PipelineBaton_GetWidthPost(PipelineBaton* baton);
  void PipelineBaton_SetWidthPost(PipelineBaton* baton, int val);
  int PipelineBaton_GetHeightPost(PipelineBaton* baton);
  void PipelineBaton_SetHeightPost(PipelineBaton* baton, int val);
  int PipelineBaton_GetWidth(PipelineBaton* baton);
  void PipelineBaton_SetWidth(PipelineBaton* baton, int val);
  int PipelineBaton_GetHeight(PipelineBaton* baton);
  void PipelineBaton_SetHeight(PipelineBaton* baton, int val);
  int PipelineBaton_GetChannels(PipelineBaton* baton);
  void PipelineBaton_SetChannels(PipelineBaton* baton, int val);
  sharp::Canvas PipelineBaton_GetCanvas(PipelineBaton* baton);
  void PipelineBaton_SetCanvas(PipelineBaton* baton, sharp::Canvas val);
  int PipelineBaton_GetPosition(PipelineBaton* baton);
  void PipelineBaton_SetPosition(PipelineBaton* baton, int val);
  double* PipelineBaton_GetResizeBackground(PipelineBaton* baton);
  void PipelineBaton_SetResizeBackground(PipelineBaton* baton, std::vector<double> val);
  bool PipelineBaton_GetHasCropOffset(PipelineBaton* baton);
  void PipelineBaton_SetHasCropOffset(PipelineBaton* baton, bool val);
  int PipelineBaton_GetCropOffsetLeft(PipelineBaton* baton);
  void PipelineBaton_SetCropOffsetLeft(PipelineBaton* baton, int val);
  int PipelineBaton_GetCropOffsetTop(PipelineBaton* baton);
  void PipelineBaton_SetCropOffsetTop(PipelineBaton* baton, int val);
  bool PipelineBaton_GetPremultiplied(PipelineBaton* baton);
  void PipelineBaton_SetPremultiplied(PipelineBaton* baton, bool val);
  bool PipelineBaton_GetTileCentre(PipelineBaton* baton);
  void PipelineBaton_SetTileCentre(PipelineBaton* baton, bool val);
  const char* PipelineBaton_GetKernel(PipelineBaton* baton);
  void PipelineBaton_SetKernel(PipelineBaton* baton, const char* val);
  bool PipelineBaton_GetFastShrinkOnLoad(PipelineBaton* baton);
  void PipelineBaton_SetFastShrinkOnLoad(PipelineBaton* baton, bool val);
  double PipelineBaton_GetTintA(PipelineBaton* baton);
  void PipelineBaton_SetTintA(PipelineBaton* baton, double val);
  double PipelineBaton_GetTintB(PipelineBaton* baton);
  void PipelineBaton_SetTintB(PipelineBaton* baton, double val);
  bool PipelineBaton_GetFlatten(PipelineBaton* baton);
  void PipelineBaton_SetFlatten(PipelineBaton* baton, bool val);
  double* PipelineBaton_GetFlattenBackground(PipelineBaton* baton);
  void PipelineBaton_SetFlattenBackground(PipelineBaton* baton, std::vector<double> val);
  bool PipelineBaton_GetNegate(PipelineBaton* baton);
  void PipelineBaton_SetNegate(PipelineBaton* baton, bool val);
  bool PipelineBaton_GetNegateAlpha(PipelineBaton* baton);
  void PipelineBaton_SetNegateAlpha(PipelineBaton* baton, bool val);
  double PipelineBaton_GetBlurSigma(PipelineBaton* baton);
  void PipelineBaton_SetBlurSigma(PipelineBaton* baton, double val);
  double PipelineBaton_GetBrightness(PipelineBaton* baton);
  void PipelineBaton_SetBrightness(PipelineBaton* baton, double val);
  double PipelineBaton_GetSaturation(PipelineBaton* baton);
  void PipelineBaton_SetSaturation(PipelineBaton* baton, double val);
  int PipelineBaton_GetHue(PipelineBaton* baton);
  void PipelineBaton_SetHue(PipelineBaton* baton, int val);
  double PipelineBaton_GetLightness(PipelineBaton* baton);
  void PipelineBaton_SetLightness(PipelineBaton* baton, double val);
  int PipelineBaton_GetMedianSize(PipelineBaton* baton);
  void PipelineBaton_SetMedianSize(PipelineBaton* baton, int val);
  double PipelineBaton_GetSharpenSigma(PipelineBaton* baton);
  void PipelineBaton_SetSharpenSigma(PipelineBaton* baton, double val);
  double PipelineBaton_GetSharpenM1(PipelineBaton* baton);
  void PipelineBaton_SetSharpenM1(PipelineBaton* baton, double val);
  double PipelineBaton_GetSharpenM2(PipelineBaton* baton);
  void PipelineBaton_SetSharpenM2(PipelineBaton* baton, double val);
  double PipelineBaton_GetSharpenX1(PipelineBaton* baton);
  void PipelineBaton_SetSharpenX1(PipelineBaton* baton, double val);
  double PipelineBaton_GetSharpenY2(PipelineBaton* baton);
  void PipelineBaton_SetSharpenY2(PipelineBaton* baton, double val);
  double PipelineBaton_GetSharpenY3(PipelineBaton* baton);
  void PipelineBaton_SetSharpenY3(PipelineBaton* baton, double val);
  int PipelineBaton_GetThreshold(PipelineBaton* baton);
  void PipelineBaton_SetThreshold(PipelineBaton* baton, int val);
  bool PipelineBaton_GetThresholdGrayscale(PipelineBaton* baton);
  void PipelineBaton_SetThresholdGrayscale(PipelineBaton* baton, bool val);
  double PipelineBaton_GetTrimThreshold(PipelineBaton* baton);
  void PipelineBaton_SetTrimThreshold(PipelineBaton* baton, double val);
  int PipelineBaton_GetTrimOffsetLeft(PipelineBaton* baton);
  void PipelineBaton_SetTrimOffsetLeft(PipelineBaton* baton, int val);
  int PipelineBaton_GetTrimOffsetTop(PipelineBaton* baton);
  void PipelineBaton_SetTrimOffsetTop(PipelineBaton* baton, int val);
  double PipelineBaton_GetLinearA(PipelineBaton* baton);
  void PipelineBaton_SetLinearA(PipelineBaton* baton, double val);
  double PipelineBaton_GetLinearB(PipelineBaton* baton);
  void PipelineBaton_SetLinearB(PipelineBaton* baton, double val);
  double PipelineBaton_GetGamma(PipelineBaton* baton);
  void PipelineBaton_SetGamma(PipelineBaton* baton, double val);
  double PipelineBaton_GetGammaOut(PipelineBaton* baton);
  void PipelineBaton_SetGammaOut(PipelineBaton* baton, double val);
  bool PipelineBaton_GetGreyscale(PipelineBaton* baton);
  void PipelineBaton_SetGreyscale(PipelineBaton* baton, bool val);
  bool PipelineBaton_GetNormalise(PipelineBaton* baton);
  void PipelineBaton_SetNormalise(PipelineBaton* baton, bool val);
  int PipelineBaton_GetClaheWidth(PipelineBaton* baton);
  void PipelineBaton_SetClaheWidth(PipelineBaton* baton, int val);
  int PipelineBaton_GetClaheHeight(PipelineBaton* baton);
  void PipelineBaton_SetClaheHeight(PipelineBaton* baton, int val);
  int PipelineBaton_GetClaheMaxSlope(PipelineBaton* baton);
  void PipelineBaton_SetClaheMaxSlope(PipelineBaton* baton, int val);
  bool PipelineBaton_GetUseExifOrientation(PipelineBaton* baton);
  void PipelineBaton_SetUseExifOrientation(PipelineBaton* baton, bool val);
  int PipelineBaton_GetAngle(PipelineBaton* baton);
  void PipelineBaton_SetAngle(PipelineBaton* baton, int val);
  double PipelineBaton_GetRotationAngle(PipelineBaton* baton);
  void PipelineBaton_SetRotationAngle(PipelineBaton* baton, double val);
  double* PipelineBaton_GetRotationBackground(PipelineBaton* baton);
  void PipelineBaton_SetRotationBackground(PipelineBaton* baton, std::vector<double> val);
  bool PipelineBaton_GetRotateBeforePreExtract(PipelineBaton* baton);
  void PipelineBaton_SetRotateBeforePreExtract(PipelineBaton* baton, bool val);
  bool PipelineBaton_GetFlip(PipelineBaton* baton);
  void PipelineBaton_SetFlip(PipelineBaton* baton, bool val);
  bool PipelineBaton_GetFlop(PipelineBaton* baton);
  void PipelineBaton_SetFlop(PipelineBaton* baton, bool val);
  int PipelineBaton_GetExtendTop(PipelineBaton* baton);
  void PipelineBaton_SetExtendTop(PipelineBaton* baton, int val);
  int PipelineBaton_GetExtendBottom(PipelineBaton* baton);
  void PipelineBaton_SetExtendBottom(PipelineBaton* baton, int val);
  int PipelineBaton_GetExtendLeft(PipelineBaton* baton);
  void PipelineBaton_SetExtendLeft(PipelineBaton* baton, int val);
  int PipelineBaton_GetExtendRight(PipelineBaton* baton);
  void PipelineBaton_SetExtendRight(PipelineBaton* baton, int val);
  double* PipelineBaton_GetExtendBackground(PipelineBaton* baton);
  void PipelineBaton_SetExtendBackground(PipelineBaton* baton, std::vector<double> val);
  bool PipelineBaton_GetWithoutEnlargement(PipelineBaton* baton);
  void PipelineBaton_SetWithoutEnlargement(PipelineBaton* baton, bool val);
  bool PipelineBaton_GetWithoutReduction(PipelineBaton* baton);
  void PipelineBaton_SetWithoutReduction(PipelineBaton* baton, bool val);
  double* PipelineBaton_GetAffineMatrix(PipelineBaton* baton);
  void PipelineBaton_SetAffineMatrix(PipelineBaton* baton, std::vector<double> val);
  double* PipelineBaton_GetAffineBackground(PipelineBaton* baton);
  void PipelineBaton_SetAffineBackground(PipelineBaton* baton, std::vector<double> val);
  double PipelineBaton_GetAffineIdx(PipelineBaton* baton);
  void PipelineBaton_SetAffineIdx(PipelineBaton* baton, double val);
  double PipelineBaton_GetAffineIdy(PipelineBaton* baton);
  void PipelineBaton_SetAffineIdy(PipelineBaton* baton, double val);
  double PipelineBaton_GetAffineOdx(PipelineBaton* baton);
  void PipelineBaton_SetAffineOdx(PipelineBaton* baton, double val);
  double PipelineBaton_GetAffineOdy(PipelineBaton* baton);
  void PipelineBaton_SetAffineOdy(PipelineBaton* baton, double val);
  const char* PipelineBaton_GetAffineInterpolator(PipelineBaton* baton);
  void PipelineBaton_SetAffineInterpolator(PipelineBaton* baton, const char* val);
  int PipelineBaton_GetJpegQuality(PipelineBaton* baton);
  void PipelineBaton_SetJpegQuality(PipelineBaton* baton, int val);
  bool PipelineBaton_GetJpegProgressive(PipelineBaton* baton);
  void PipelineBaton_SetJpegProgressive(PipelineBaton* baton, bool val);
  const char* PipelineBaton_GetJpegChromaSubsampling(PipelineBaton* baton);
  void PipelineBaton_SetJpegChromaSubsampling(PipelineBaton* baton, const char* val);
  bool PipelineBaton_GetJpegTrellisQuantisation(PipelineBaton* baton);
  void PipelineBaton_SetJpegTrellisQuantisation(PipelineBaton* baton, bool val);
  int PipelineBaton_GetJpegQuantisationTable(PipelineBaton* baton);
  void PipelineBaton_SetJpegQuantisationTable(PipelineBaton* baton, int val);
  bool PipelineBaton_GetJpegOvershootDeringing(PipelineBaton* baton);
  void PipelineBaton_SetJpegOvershootDeringing(PipelineBaton* baton, bool val);
  bool PipelineBaton_GetJpegOptimiseScans(PipelineBaton* baton);
  void PipelineBaton_SetJpegOptimiseScans(PipelineBaton* baton, bool val);
  bool PipelineBaton_GetJpegOptimiseCoding(PipelineBaton* baton);
  void PipelineBaton_SetJpegOptimiseCoding(PipelineBaton* baton, bool val);
  bool PipelineBaton_GetPngProgressive(PipelineBaton* baton);
  void PipelineBaton_SetPngProgressive(PipelineBaton* baton, bool val);
  int PipelineBaton_GetPngCompressionLevel(PipelineBaton* baton);
  void PipelineBaton_SetPngCompressionLevel(PipelineBaton* baton, int val);
  bool PipelineBaton_GetPngAdaptiveFiltering(PipelineBaton* baton);
  void PipelineBaton_SetPngAdaptiveFiltering(PipelineBaton* baton, bool val);
  bool PipelineBaton_GetPngPalette(PipelineBaton* baton);
  void PipelineBaton_SetPngPalette(PipelineBaton* baton, bool val);
  int PipelineBaton_GetPngQuality(PipelineBaton* baton);
  void PipelineBaton_SetPngQuality(PipelineBaton* baton, int val);
  int PipelineBaton_GetPngEffort(PipelineBaton* baton);
  void PipelineBaton_SetPngEffort(PipelineBaton* baton, int val);
  int PipelineBaton_GetPngBitdepth(PipelineBaton* baton);
  void PipelineBaton_SetPngBitdepth(PipelineBaton* baton, int val);
  double PipelineBaton_GetPngDither(PipelineBaton* baton);
  void PipelineBaton_SetPngDither(PipelineBaton* baton, double val);
  int PipelineBaton_GetJp2Quality(PipelineBaton* baton);
  void PipelineBaton_SetJp2Quality(PipelineBaton* baton, int val);
  bool PipelineBaton_GetJp2Lossless(PipelineBaton* baton);
  void PipelineBaton_SetJp2Lossless(PipelineBaton* baton, bool val);
  int PipelineBaton_GetJp2TileHeight(PipelineBaton* baton);
  void PipelineBaton_SetJp2TileHeight(PipelineBaton* baton, int val);
  int PipelineBaton_GetJp2TileWidth(PipelineBaton* baton);
  void PipelineBaton_SetJp2TileWidth(PipelineBaton* baton, int val);
  const char* PipelineBaton_GetJp2ChromaSubsampling(PipelineBaton* baton);
  void PipelineBaton_SetJp2ChromaSubsampling(PipelineBaton* baton, const char* val);
  int PipelineBaton_GetWebpQuality(PipelineBaton* baton);
  void PipelineBaton_SetWebpQuality(PipelineBaton* baton, int val);
  int PipelineBaton_GetWebpAlphaQuality(PipelineBaton* baton);
  void PipelineBaton_SetWebpAlphaQuality(PipelineBaton* baton, int val);
  bool PipelineBaton_GetWebpNearLossless(PipelineBaton* baton);
  void PipelineBaton_SetWebpNearLossless(PipelineBaton* baton, bool val);
  bool PipelineBaton_GetWebpLossless(PipelineBaton* baton);
  void PipelineBaton_SetWebpLossless(PipelineBaton* baton, bool val);
  bool PipelineBaton_GetWebpSmartSubsample(PipelineBaton* baton);
  void PipelineBaton_SetWebpSmartSubsample(PipelineBaton* baton, bool val);
  int PipelineBaton_GetWebpEffort(PipelineBaton* baton);
  void PipelineBaton_SetWebpEffort(PipelineBaton* baton, int val);
  int PipelineBaton_GetGifBitdepth(PipelineBaton* baton);
  void PipelineBaton_SetGifBitdepth(PipelineBaton* baton, int val);
  int PipelineBaton_GetGifEffort(PipelineBaton* baton);
  void PipelineBaton_SetGifEffort(PipelineBaton* baton, int val);
  double PipelineBaton_GetGifDither(PipelineBaton* baton);
  void PipelineBaton_SetGifDither(PipelineBaton* baton, double val);
  int PipelineBaton_GetTiffQuality(PipelineBaton* baton);
  void PipelineBaton_SetTiffQuality(PipelineBaton* baton, int val);
  VipsForeignTiffCompression PipelineBaton_GetTiffCompression(PipelineBaton* baton);
  void PipelineBaton_SetTiffCompression(PipelineBaton* baton, VipsForeignTiffCompression val);
  VipsForeignTiffPredictor PipelineBaton_GetTiffPredictor(PipelineBaton* baton);
  void PipelineBaton_SetTiffPredictor(PipelineBaton* baton, VipsForeignTiffPredictor val);
  bool PipelineBaton_GetTiffPyramid(PipelineBaton* baton);
  void PipelineBaton_SetTiffPyramid(PipelineBaton* baton, bool val);
  int PipelineBaton_GetTiffBitdepth(PipelineBaton* baton);
  void PipelineBaton_SetTiffBitdepth(PipelineBaton* baton, int val);
  bool PipelineBaton_GetTiffTile(PipelineBaton* baton);
  void PipelineBaton_SetTiffTile(PipelineBaton* baton, bool val);
  int PipelineBaton_GetTiffTileHeight(PipelineBaton* baton);
  void PipelineBaton_SetTiffTileHeight(PipelineBaton* baton, int val);
  int PipelineBaton_GetTiffTileWidth(PipelineBaton* baton);
  void PipelineBaton_SetTiffTileWidth(PipelineBaton* baton, int val);
  double PipelineBaton_GetTiffXres(PipelineBaton* baton);
  void PipelineBaton_SetTiffXres(PipelineBaton* baton, double val);
  double PipelineBaton_GetTiffYres(PipelineBaton* baton);
  void PipelineBaton_SetTiffYres(PipelineBaton* baton, double val);
  VipsForeignTiffResunit PipelineBaton_GetTiffResolutionUnit(PipelineBaton* baton);
  void PipelineBaton_SetTiffResolutionUnit(PipelineBaton* baton, VipsForeignTiffResunit val);
  int PipelineBaton_GetHeifQuality(PipelineBaton* baton);
  void PipelineBaton_SetHeifQuality(PipelineBaton* baton, int val);
  VipsForeignHeifCompression PipelineBaton_GetHeifCompression(PipelineBaton* baton);
  void PipelineBaton_SetHeifCompression(PipelineBaton* baton, VipsForeignHeifCompression val);
  int PipelineBaton_GetHeifEffort(PipelineBaton* baton);
  void PipelineBaton_SetHeifEffort(PipelineBaton* baton, int val);
  const char* PipelineBaton_GetHeifChromaSubsampling(PipelineBaton* baton);
  void PipelineBaton_SetHeifChromaSubsampling(PipelineBaton* baton, const char* val);
  bool PipelineBaton_GetHeifLossless(PipelineBaton* baton);
  void PipelineBaton_SetHeifLossless(PipelineBaton* baton, bool val);
  VipsBandFormat PipelineBaton_GetRawDepth(PipelineBaton* baton);
  void PipelineBaton_SetRawDepth(PipelineBaton* baton, VipsBandFormat val);
  const char* PipelineBaton_GetErr(PipelineBaton* baton);
  void PipelineBaton_SetErr(PipelineBaton* baton, const char* val);
  bool PipelineBaton_GetWithMetadata(PipelineBaton* baton);
  void PipelineBaton_SetWithMetadata(PipelineBaton* baton, bool val);
  int PipelineBaton_GetWithMetadataOrientation(PipelineBaton* baton);
  void PipelineBaton_SetWithMetadataOrientation(PipelineBaton* baton, int val);
  double PipelineBaton_GetWithMetadataDensity(PipelineBaton* baton);
  void PipelineBaton_SetWithMetadataDensity(PipelineBaton* baton, double val);
  const char* PipelineBaton_GetWithMetadataIcc(PipelineBaton* baton);
  void PipelineBaton_SetWithMetadataIcc(PipelineBaton* baton, const char* val);
  std::unordered_map<std::string, std::string> PipelineBaton_GetWithMetadataStrs(PipelineBaton* baton);
  void PipelineBaton_SetWithMetadataStrs(PipelineBaton* baton, std::unordered_map<std::string, std::string> val);
  int PipelineBaton_GetTimeoutSeconds(PipelineBaton* baton);
  void PipelineBaton_SetTimeoutSeconds(PipelineBaton* baton, int val);
  double* PipelineBaton_GetConvKernel(PipelineBaton* baton);
  void PipelineBaton_SetConvKernel(PipelineBaton* baton, std::unique_ptr<double[]> val);
  int PipelineBaton_GetConvKernelWidth(PipelineBaton* baton);
  void PipelineBaton_SetConvKernelWidth(PipelineBaton* baton, int val);
  int PipelineBaton_GetConvKernelHeight(PipelineBaton* baton);
  void PipelineBaton_SetConvKernelHeight(PipelineBaton* baton, int val);
  double PipelineBaton_GetConvKernelScale(PipelineBaton* baton);
  void PipelineBaton_SetConvKernelScale(PipelineBaton* baton, double val);
  double PipelineBaton_GetConvKernelOffset(PipelineBaton* baton);
  void PipelineBaton_SetConvKernelOffset(PipelineBaton* baton, double val);
  InputDescriptor* PipelineBaton_GetBoolean(PipelineBaton* baton);
  void PipelineBaton_SetBoolean(PipelineBaton* baton, InputDescriptor* val);
  VipsOperationBoolean PipelineBaton_GetBooleanOp(PipelineBaton* baton);
  void PipelineBaton_SetBooleanOp(PipelineBaton* baton, VipsOperationBoolean val);
  VipsOperationBoolean PipelineBaton_GetBandBoolOp(PipelineBaton* baton);
  void PipelineBaton_SetBandBoolOp(PipelineBaton* baton, VipsOperationBoolean val);
  int PipelineBaton_GetExtractChannel(PipelineBaton* baton);
  void PipelineBaton_SetExtractChannel(PipelineBaton* baton, int val);
  bool PipelineBaton_GetRemoveAlpha(PipelineBaton* baton);
  void PipelineBaton_SetRemoveAlpha(PipelineBaton* baton, bool val);
  double PipelineBaton_GetEnsureAlpha(PipelineBaton* baton);
  void PipelineBaton_SetEnsureAlpha(PipelineBaton* baton, double val);
  VipsInterpretation PipelineBaton_GetColourspaceInput(PipelineBaton* baton);
  void PipelineBaton_SetColourspaceInput(PipelineBaton* baton, VipsInterpretation val);
  VipsInterpretation PipelineBaton_GetColourspace(PipelineBaton* baton);
  void PipelineBaton_SetColourspace(PipelineBaton* baton, VipsInterpretation val);
  std::vector<int> PipelineBaton_GetDelay(PipelineBaton* baton);
  void PipelineBaton_SetDelay(PipelineBaton* baton, std::vector<int> val);
  int PipelineBaton_GetLoop(PipelineBaton* baton);
  void PipelineBaton_SetLoop(PipelineBaton* baton, int val);
  int PipelineBaton_GetTileSize(PipelineBaton* baton);
  void PipelineBaton_SetTileSize(PipelineBaton* baton, int val);
  int PipelineBaton_GetTileOverlap(PipelineBaton* baton);
  void PipelineBaton_SetTileOverlap(PipelineBaton* baton, int val);
  VipsForeignDzContainer PipelineBaton_GetTileContainer(PipelineBaton* baton);
  void PipelineBaton_SetTileContainer(PipelineBaton* baton, VipsForeignDzContainer val);
  VipsForeignDzLayout PipelineBaton_GetTileLayout(PipelineBaton* baton);
  void PipelineBaton_SetTileLayout(PipelineBaton* baton, VipsForeignDzLayout val);
  const char* PipelineBaton_GetTileFormat(PipelineBaton* baton);
  void PipelineBaton_SetTileFormat(PipelineBaton* baton, const char* val);
  int PipelineBaton_GetTileAngle(PipelineBaton* baton);
  void PipelineBaton_SetTileAngle(PipelineBaton* baton, int val);
  double* PipelineBaton_GetTileBackground(PipelineBaton* baton);
  void PipelineBaton_SetTileBackground(PipelineBaton* baton, std::vector<double> val);
  int PipelineBaton_GetTileSkipBlanks(PipelineBaton* baton);
  void PipelineBaton_SetTileSkipBlanks(PipelineBaton* baton, int val);
  VipsForeignDzDepth PipelineBaton_GetTileDepth(PipelineBaton* baton);
  void PipelineBaton_SetTileDepth(PipelineBaton* baton, VipsForeignDzDepth val);
  const char* PipelineBaton_GetTileId(PipelineBaton* baton);
  void PipelineBaton_SetTileId(PipelineBaton* baton, const char* val);
  double* PipelineBaton_GetRecombMatrix(PipelineBaton* baton);
  void PipelineBaton_SetRecombMatrix(PipelineBaton* baton, std::unique_ptr<double[]> val);

  void PipelineBaton_Composite_PushBack(PipelineBaton* baton, Composite * value);
  void PipelineBaton_JoinChannelIn_PushBack(PipelineBaton* baton, InputDescriptor * value);
  void PipelineBaton_ResizeBackground_PushBack(PipelineBaton* baton, double value);
  void PipelineBaton_FlattenBackground_PushBack(PipelineBaton* baton, double value);
  void PipelineBaton_RotationBackground_PushBack(PipelineBaton* baton, double value);
  void PipelineBaton_ExtendBackground_PushBack(PipelineBaton* baton, double value);
  void PipelineBaton_AffineMatrix_PushBack(PipelineBaton* baton, double value);
  void PipelineBaton_AffineBackground_PushBack(PipelineBaton* baton, double value);
  void PipelineBaton_Delay_PushBack(PipelineBaton* baton, int value);
  void PipelineBaton_TileBackground_PushBack(PipelineBaton* baton, double value);

  void PipelineBaton_WithMetadataStrs_Insert(PipelineBaton* baton, std::pair<std::string, std::string> value);
}

#endif  // SRC_PIPELINE_SANDBOX_H_
