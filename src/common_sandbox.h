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

#ifndef SRC_COMMON_SANDBOX_H_
#define SRC_COMMON_SANDBOX_H_

#include <string>
#include <tuple>
#include <vector>
#include <functional>

#include <vips/vips8>

#include "canvas.h"

// Verify platform and compiler compatibility

#if (VIPS_MAJOR_VERSION < 8) || \
  (VIPS_MAJOR_VERSION == 8 && VIPS_MINOR_VERSION < 12) || \
  (VIPS_MAJOR_VERSION == 8 && VIPS_MINOR_VERSION == 12 && VIPS_MICRO_VERSION < 2)
#error "libvips version 8.12.2+ is required - please see https://sharp.pixelplumbing.com/install"
#endif

#if ((!defined(__clang__)) && defined(__GNUC__) && (__GNUC__ < 4 || (__GNUC__ == 4 && __GNUC_MINOR__ < 6)))
#error "GCC version 4.6+ is required for C++11 features - please see https://sharp.pixelplumbing.com/install"
#endif

#if (defined(__clang__) && defined(__has_feature))
#if (!__has_feature(cxx_range_for))
#error "clang version 3.0+ is required for C++11 features - please see https://sharp.pixelplumbing.com/install"
#endif
#endif

using vips::VImage;

struct InputDescriptor {  // NOLINT(runtime/indentation_namespace)
  std::string name;
  std::string file;
  char *buffer;
  bool failOnError;
  int limitInputPixels;
  bool unlimited;
  VipsAccess access;
  size_t bufferLength;
  bool isBuffer;
  double density;
  VipsBandFormat rawDepth;
  int rawChannels;
  int rawWidth;
  int rawHeight;
  bool rawPremultiplied;
  int pages;
  int page;
  int level;
  int subifd;
  int createChannels;
  int createWidth;
  int createHeight;
  std::vector<double> createBackground;
  std::string createNoiseType;
  double createNoiseMean;
  double createNoiseSigma;

  InputDescriptor():
    buffer(nullptr),
    failOnError(TRUE),
    limitInputPixels(0x3FFF * 0x3FFF),
    unlimited(FALSE),
    access(VIPS_ACCESS_RANDOM),
    bufferLength(0),
    isBuffer(FALSE),
    density(72.0),
    rawDepth(VIPS_FORMAT_UCHAR),
    rawChannels(0),
    rawWidth(0),
    rawHeight(0),
    rawPremultiplied(false),
    pages(1),
    page(0),
    level(0),
    subifd(-1),
    createChannels(0),
    createWidth(0),
    createHeight(0),
    createBackground{ 0.0, 0.0, 0.0, 255.0 },
    createNoiseMean(0.0),
    createNoiseSigma(0.0) {}
};


extern "C" {
  InputDescriptor* CreateEmptyInputDescriptor();
  void DestroyInputDescriptor(InputDescriptor* input);

  const char* InputDescriptor_GetName(InputDescriptor* input);
  void InputDescriptor_SetName(InputDescriptor* input, const char* val);
  const char* InputDescriptor_GetFile(InputDescriptor* input);
  void InputDescriptor_SetFile(InputDescriptor* input, const char* val);
  char* InputDescriptor_GetBuffer(InputDescriptor* input);
  void InputDescriptor_SetBuffer(InputDescriptor* input, char* val);
  bool InputDescriptor_GetFailOnError(InputDescriptor* input);
  void InputDescriptor_SetFailOnError(InputDescriptor* input, bool val);
  int InputDescriptor_GetLimitInputPixels(InputDescriptor* input);
  void InputDescriptor_SetLimitInputPixels(InputDescriptor* input, int val);
  bool InputDescriptor_GetUnlimited(InputDescriptor* input);
  void InputDescriptor_SetUnlimited(InputDescriptor* input, bool val);
  int InputDescriptor_GetAccess(InputDescriptor* input);
  void InputDescriptor_SetAccess(InputDescriptor* input, int val);
  size_t InputDescriptor_GetBufferLength(InputDescriptor* input);
  void InputDescriptor_SetBufferLength(InputDescriptor* input, size_t val);
  bool InputDescriptor_GetIsBuffer(InputDescriptor* input);
  void InputDescriptor_SetIsBuffer(InputDescriptor* input, bool val);
  double InputDescriptor_GetDensity(InputDescriptor* input);
  void InputDescriptor_SetDensity(InputDescriptor* input, double val);
  int InputDescriptor_GetRawDepth(InputDescriptor* input);
  void InputDescriptor_SetRawDepth(InputDescriptor* input, int val);
  int InputDescriptor_GetRawChannels(InputDescriptor* input);
  void InputDescriptor_SetRawChannels(InputDescriptor* input, int val);
  int InputDescriptor_GetRawWidth(InputDescriptor* input);
  void InputDescriptor_SetRawWidth(InputDescriptor* input, int val);
  int InputDescriptor_GetRawHeight(InputDescriptor* input);
  void InputDescriptor_SetRawHeight(InputDescriptor* input, int val);
  bool InputDescriptor_GetRawPremultiplied(InputDescriptor* input);
  void InputDescriptor_SetRawPremultiplied(InputDescriptor* input, bool val);
  int InputDescriptor_GetPages(InputDescriptor* input);
  void InputDescriptor_SetPages(InputDescriptor* input, int val);
  int InputDescriptor_GetPage(InputDescriptor* input);
  void InputDescriptor_SetPage(InputDescriptor* input, int val);
  int InputDescriptor_GetLevel(InputDescriptor* input);
  void InputDescriptor_SetLevel(InputDescriptor* input, int val);
  int InputDescriptor_GetSubifd(InputDescriptor* input);
  void InputDescriptor_SetSubifd(InputDescriptor* input, int val);
  int InputDescriptor_GetCreateChannels(InputDescriptor* input);
  void InputDescriptor_SetCreateChannels(InputDescriptor* input, int val);
  int InputDescriptor_GetCreateWidth(InputDescriptor* input);
  void InputDescriptor_SetCreateWidth(InputDescriptor* input, int val);
  int InputDescriptor_GetCreateHeight(InputDescriptor* input);
  void InputDescriptor_SetCreateHeight(InputDescriptor* input, int val);
  double* InputDescriptor_GetCreateBackground(InputDescriptor* input);
  void InputDescriptor_SetCreateBackground(InputDescriptor* input, double* val, size_t count);
  const char* InputDescriptor_GetCreateNoiseType(InputDescriptor* input);
  void InputDescriptor_SetCreateNoiseType(InputDescriptor* input, const char* val);
  double InputDescriptor_GetCreateNoiseMean(InputDescriptor* input);
  void InputDescriptor_SetCreateNoiseMean(InputDescriptor* input, double val);
  double InputDescriptor_GetCreateNoiseSigma(InputDescriptor* input);
  void InputDescriptor_SetCreateNoiseSigma(InputDescriptor* input, double val);
}

namespace sharp {
  enum class ImageType {
    JPEG,
    PNG,
    WEBP,
    JP2,
    TIFF,
    GIF,
    SVG,
    HEIF,
    PDF,
    MAGICK,
    OPENSLIDE,
    PPM,
    FITS,
    EXR,
    VIPS,
    RAW,
    UNKNOWN,
    MISSING
  };

  // Filename extension checkers
  bool IsJpeg(std::string const &str);
  bool IsPng(std::string const &str);
  bool IsWebp(std::string const &str);
  bool IsJp2(std::string const &str);
  bool IsGif(std::string const &str);
  bool IsTiff(std::string const &str);
  bool IsHeic(std::string const &str);
  bool IsHeif(std::string const &str);
  bool IsAvif(std::string const &str);
  bool IsDz(std::string const &str);
  bool IsDzZip(std::string const &str);
  bool IsV(std::string const &str);

  /*
    Provide a string identifier for the given image type.
  */
  std::string ImageTypeId(ImageType const imageType);

  /*
    Determine image format of a buffer.
  */
  ImageType DetermineImageType(void *buffer, size_t const length);

  /*
    Determine image format of a file.
  */
  ImageType DetermineImageType(char const *file);

  /*
    Does this image type support multiple pages?
  */
  bool ImageTypeSupportsPage(ImageType imageType);

  /*
    Open an image from the given InputDescriptor (filesystem, compressed buffer, raw pixel data)
  */
  std::tuple<VImage, ImageType> OpenInput(InputDescriptor *descriptor);

  /*
    Does this image have an embedded profile?
  */
  bool HasProfile(VImage image);

  /*
    Does this image have an alpha channel?
    Uses colour space interpretation with number of channels to guess this.
  */
  bool HasAlpha(VImage image);

  /*
    Get EXIF Orientation of image, if any.
  */
  int ExifOrientation(VImage image);

  /*
    Set EXIF Orientation of image.
  */
  VImage SetExifOrientation(VImage image, int const orientation);

  /*
    Remove EXIF Orientation from image.
  */
  VImage RemoveExifOrientation(VImage image);

  /*
    Set animation properties if necessary.
  */
  VImage SetAnimationProperties(VImage image, int nPages, int pageHeight, std::vector<int> delay, int loop);

  /*
    Remove animation properties from image.
  */
  VImage RemoveAnimationProperties(VImage image);

  /*
    Does this image have a non-default density?
  */
  bool HasDensity(VImage image);

  /*
    Get pixels/mm resolution as pixels/inch density.
  */
  int GetDensity(VImage image);

  /*
    Set pixels/mm resolution based on a pixels/inch density.
  */
  VImage SetDensity(VImage image, const double density);

  /*
    Multi-page images can have a page height. Fetch it, and sanity check it.
    If page-height is not set, it defaults to the image height
  */
  int GetPageHeight(VImage image);

  /*
    Check the proposed format supports the current dimensions.
  */
  void AssertImageTypeDimensions(VImage image, ImageType const imageType);

  /*
    Attach an event listener for progress updates, used to detect timeout
  */
  void SetTimeout(VImage image, int const timeoutSeconds);

  /*
    Event listener for progress updates, used to detect timeout
  */
  void VipsProgressCallBack(VipsImage *image, VipsProgress *progress, int *timeoutSeconds);

  /*
    Calculate the (left, top) coordinates of the output image
    within the input image, applying the given gravity during an embed.
  */
  std::tuple<int, int> CalculateEmbedPosition(int const inWidth, int const inHeight,
    int const outWidth, int const outHeight, int const gravity);

  /*
    Calculate the (left, top) coordinates of the output image
    within the input image, applying the given gravity.
  */
  std::tuple<int, int> CalculateCrop(int const inWidth, int const inHeight,
    int const outWidth, int const outHeight, int const gravity);

  /*
    Calculate the (left, top) coordinates of the output image
    within the input image, applying the given x and y offsets of the output image.
  */
  std::tuple<int, int> CalculateCrop(int const inWidth, int const inHeight,
    int const outWidth, int const outHeight, int const x, int const y);

  /*
    Are pixel values in this image 16-bit integer?
  */
  bool Is16Bit(VipsInterpretation const interpretation);

  /*
    Return the image alpha maximum. Useful for combining alpha bands. scRGB
    images are 0 - 1 for image data, but the alpha is 0 - 255.
  */
  double MaximumImageAlpha(VipsInterpretation const interpretation);

  /*
    Get boolean operation type from string
  */
  VipsOperationBoolean GetBooleanOperation(std::string const opStr);

  /*
    Get interpretation type from string
  */
  VipsInterpretation GetInterpretation(std::string const typeStr);

  /*
    Convert RGBA value to another colourspace
  */
  std::vector<double> GetRgbaAsColourspace(std::vector<double> const rgba,
    VipsInterpretation const interpretation, bool premultiply);

  /*
    Apply the alpha channel to a given colour
   */
  std::tuple<VImage, std::vector<double>> ApplyAlpha(VImage image, std::vector<double> colour, bool premultiply);

  /*
    Removes alpha channel, if any.
  */
  VImage RemoveAlpha(VImage image);

  /*
    Ensures alpha channel, if missing.
  */
  VImage EnsureAlpha(VImage image, double const value);

  /*
    Calculate the shrink factor, taking into account auto-rotate, the canvas
    mode, and so on. The hshrink/vshrink are the amount to shrink the input
    image axes by in order for the output axes (ie. after rotation) to match
    the required thumbnail width/height and canvas mode.
  */
  std::pair<double, double> ResolveShrink(int width, int height, int targetWidth, int targetHeight,
    Canvas canvas, bool swap, bool withoutEnlargement, bool withoutReduction);

}  // namespace sharp

#endif  // SRC_COMMON_SANDBOX_H_
