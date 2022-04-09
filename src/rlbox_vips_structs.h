#pragma once

#include <map>
#include <string>

using unordered_map_string_string = std::unordered_map<std::string, std::string>;

#define sandbox_fields_reflection_vips_class_PipelineBaton(f, g, ...) \
  f(sharp::InputDescriptor *,                     input,                   FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(std::string,                                  formatOut,               FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(std::string,                                  fileOut,                 FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(void *,                                       bufferOut,               FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(size_t,                                       bufferOutLength,         FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(std::vector<Composite *>,                     composite,               FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(std::vector<sharp::InputDescriptor *>,        joinChannelIn,           FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(int,                                          topOffsetPre,            FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(int,                                          leftOffsetPre,           FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(int,                                          widthPre,                FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(int,                                          heightPre,               FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(int,                                          topOffsetPost,           FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(int,                                          leftOffsetPost,          FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(int,                                          widthPost,               FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(int,                                          heightPost,              FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(int,                                          width,                   FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(int,                                          height,                  FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(int,                                          channels,                FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(sharp::Canvas,                                canvas,                  FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(int,                                          position,                FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(std::vector<double>,                          resizeBackground,        FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(bool,                                         hasCropOffset,           FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(int,                                          cropOffsetLeft,          FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(int,                                          cropOffsetTop,           FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(bool,                                         premultiplied,           FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(bool,                                         tileCentre,              FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(std::string,                                  kernel,                  FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(bool,                                         fastShrinkOnLoad,        FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(double,                                       tintA,                   FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(double,                                       tintB,                   FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(bool,                                         flatten,                 FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(std::vector<double>,                          flattenBackground,       FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(bool,                                         negate,                  FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(bool,                                         negateAlpha,             FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(double,                                       blurSigma,               FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(double,                                       brightness,              FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(double,                                       saturation,              FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(int,                                          hue,                     FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(double,                                       lightness,               FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(int,                                          medianSize,              FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(double,                                       sharpenSigma,            FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(double,                                       sharpenM1,               FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(double,                                       sharpenM2,               FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(double,                                       sharpenX1,               FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(double,                                       sharpenY2,               FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(double,                                       sharpenY3,               FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(int,                                          threshold,               FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(bool,                                         thresholdGrayscale,      FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(double,                                       trimThreshold,           FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(int,                                          trimOffsetLeft,          FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(int,                                          trimOffsetTop,           FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(double,                                       linearA,                 FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(double,                                       linearB,                 FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(double,                                       gamma,                   FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(double,                                       gammaOut,                FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(bool,                                         greyscale,               FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(bool,                                         normalise,               FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(int,                                          claheWidth,              FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(int,                                          claheHeight,             FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(int,                                          claheMaxSlope,           FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(bool,                                         useExifOrientation,      FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(int,                                          angle,                   FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(double,                                       rotationAngle,           FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(std::vector<double>,                          rotationBackground,      FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(bool,                                         rotateBeforePreExtract,  FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(bool,                                         flip,                    FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(bool,                                         flop,                    FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(int,                                          extendTop,               FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(int,                                          extendBottom,            FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(int,                                          extendLeft,              FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(int,                                          extendRight,             FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(std::vector<double>,                          extendBackground,        FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(bool,                                         withoutEnlargement,      FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(bool,                                         withoutReduction,        FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(std::vector<double>,                          affineMatrix,            FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(std::vector<double>,                          affineBackground,        FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(double,                                       affineIdx,               FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(double,                                       affineIdy,               FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(double,                                       affineOdx,               FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(double,                                       affineOdy,               FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(std::string,                                  affineInterpolator,      FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(int,                                          jpegQuality,             FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(bool,                                         jpegProgressive,         FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(std::string,                                  jpegChromaSubsampling,   FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(bool,                                         jpegTrellisQuantisation, FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(int,                                          jpegQuantisationTable,   FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(bool,                                         jpegOvershootDeringing,  FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(bool,                                         jpegOptimiseScans,       FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(bool,                                         jpegOptimiseCoding,      FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(bool,                                         pngProgressive,          FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(int,                                          pngCompressionLevel,     FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(bool,                                         pngAdaptiveFiltering,    FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(bool,                                         pngPalette,              FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(int,                                          pngQuality,              FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(int,                                          pngEffort,               FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(int,                                          pngBitdepth,             FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(double,                                       pngDither,               FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(int,                                          jp2Quality,              FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(bool,                                         jp2Lossless,             FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(int,                                          jp2TileHeight,           FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(int,                                          jp2TileWidth,            FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(std::string,                                  jp2ChromaSubsampling,    FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(int,                                          webpQuality,             FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(int,                                          webpAlphaQuality,        FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(bool,                                         webpNearLossless,        FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(bool,                                         webpLossless,            FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(bool,                                         webpSmartSubsample,      FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(int,                                          webpEffort,              FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(int,                                          gifBitdepth,             FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(int,                                          gifEffort,               FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(double,                                       gifDither,               FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(int,                                          tiffQuality,             FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(VipsForeignTiffCompression,                   tiffCompression,         FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(VipsForeignTiffPredictor,                     tiffPredictor,           FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(bool,                                         tiffPyramid,             FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(int,                                          tiffBitdepth,            FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(bool,                                         tiffTile,                FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(int,                                          tiffTileHeight,          FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(int,                                          tiffTileWidth,           FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(double,                                       tiffXres,                FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(double,                                       tiffYres,                FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(VipsForeignTiffResunit,                       tiffResolutionUnit,      FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(int,                                          heifQuality,             FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(VipsForeignHeifCompression,                   heifCompression,         FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(int,                                          heifEffort,              FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(std::string,                                  heifChromaSubsampling,   FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(bool,                                         heifLossless,            FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(VipsBandFormat,                               rawDepth,                FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(std::string,                                  err,                     FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(bool,                                         withMetadata,            FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(int,                                          withMetadataOrientation, FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(double,                                       withMetadataDensity,     FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(std::string,                                  withMetadataIcc,         FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(unordered_map_string_string,                  withMetadataStrs,        FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(int,                                          timeoutSeconds,          FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(std::unique_ptr<double[]>,                    convKernel,              FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(int,                                          convKernelWidth,         FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(int,                                          convKernelHeight,        FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(double,                                       convKernelScale,         FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(double,                                       convKernelOffset,        FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(sharp::InputDescriptor *,                     boolean,                 FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(VipsOperationBoolean,                         booleanOp,               FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(VipsOperationBoolean,                         bandBoolOp,              FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(int,                                          extractChannel,          FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(bool,                                         removeAlpha,             FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(double,                                       ensureAlpha,             FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(VipsInterpretation,                           colourspaceInput,        FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(VipsInterpretation,                           colourspace,             FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(std::vector<int>,                             delay,                   FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(int,                                          loop,                    FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(int,                                          tileSize,                FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(int,                                          tileOverlap,             FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(VipsForeignDzContainer,                       tileContainer,           FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(VipsForeignDzLayout,                          tileLayout,              FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(std::string,                                  tileFormat,              FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(int,                                          tileAngle,               FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(std::vector<double>,                          tileBackground,          FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(int,                                          tileSkipBlanks,          FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(VipsForeignDzDepth,                           tileDepth,               FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(std::string,                                  tileId,                  FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(std::unique_ptr<double[]>,                    recombMatrix,            FIELD_NORMAL, ##__VA_ARGS__) g()

#define sandbox_fields_reflection_vips_allClasses(f, ...) \
  f(PipelineBaton, vips, ##__VA_ARGS__)
