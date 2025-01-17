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

#include <vips/vips8>

#include "common_sandbox.h"

using vips::VImage;

extern "C" {

  InputDescriptor* CreateEmptyInputDescriptor() { return new InputDescriptor(); }
  void DestroyInputDescriptor(InputDescriptor* input) { delete input; }

  const char* InputDescriptor_GetName(InputDescriptor* input) { return input->name.c_str(); }
  void InputDescriptor_SetName(InputDescriptor* input, const char* val) { input->name = val; }
  const char* InputDescriptor_GetFile(InputDescriptor* input) { return input->file.c_str(); }
  void InputDescriptor_SetFile(InputDescriptor* input, const char* val) { input->file = val; }
  char* InputDescriptor_GetBuffer(InputDescriptor* input) { return input->buffer; }
  void InputDescriptor_SetBuffer(InputDescriptor* input, char* val) { input->buffer = val; }
  bool InputDescriptor_GetFailOnError(InputDescriptor* input) { return input->failOnError; }
  void InputDescriptor_SetFailOnError(InputDescriptor* input, bool val) { input->failOnError = val; }
  int InputDescriptor_GetLimitInputPixels(InputDescriptor* input) { return input->limitInputPixels; }
  void InputDescriptor_SetLimitInputPixels(InputDescriptor* input, int val) { input->limitInputPixels = val; }
  bool InputDescriptor_GetUnlimited(InputDescriptor* input) { return input->unlimited; }
  void InputDescriptor_SetUnlimited(InputDescriptor* input, bool val) { input->unlimited = val; }
  int InputDescriptor_GetAccess(InputDescriptor* input) { return (int) input->access; }
  void InputDescriptor_SetAccess(InputDescriptor* input, int val) { input->access = (VipsAccess) val; }
  size_t InputDescriptor_GetBufferLength(InputDescriptor* input) { return input->bufferLength; }
  void InputDescriptor_SetBufferLength(InputDescriptor* input, size_t val) { input->bufferLength = val; }
  bool InputDescriptor_GetIsBuffer(InputDescriptor* input) { return input->isBuffer; }
  void InputDescriptor_SetIsBuffer(InputDescriptor* input, bool val) { input->isBuffer = val; }
  double InputDescriptor_GetDensity(InputDescriptor* input) { return input->density; }
  void InputDescriptor_SetDensity(InputDescriptor* input, double val) { input->density = val; }
  int InputDescriptor_GetRawDepth(InputDescriptor* input) { return (int) input->rawDepth; }
  void InputDescriptor_SetRawDepth(InputDescriptor* input, int val) { input->rawDepth = (VipsBandFormat) val; }
  int InputDescriptor_GetRawChannels(InputDescriptor* input) { return input->rawChannels; }
  void InputDescriptor_SetRawChannels(InputDescriptor* input, int val) { input->rawChannels = val; }
  int InputDescriptor_GetRawWidth(InputDescriptor* input) { return input->rawWidth; }
  void InputDescriptor_SetRawWidth(InputDescriptor* input, int val) { input->rawWidth = val; }
  int InputDescriptor_GetRawHeight(InputDescriptor* input) { return input->rawHeight; }
  void InputDescriptor_SetRawHeight(InputDescriptor* input, int val) { input->rawHeight = val; }
  bool InputDescriptor_GetRawPremultiplied(InputDescriptor* input) { return input->rawPremultiplied; }
  void InputDescriptor_SetRawPremultiplied(InputDescriptor* input, bool val) { input->rawPremultiplied = val; }
  int InputDescriptor_GetPages(InputDescriptor* input) { return input->pages; }
  void InputDescriptor_SetPages(InputDescriptor* input, int val) { input->pages = val; }
  int InputDescriptor_GetPage(InputDescriptor* input) { return input->page; }
  void InputDescriptor_SetPage(InputDescriptor* input, int val) { input->page = val; }
  int InputDescriptor_GetLevel(InputDescriptor* input) { return input->level; }
  void InputDescriptor_SetLevel(InputDescriptor* input, int val) { input->level = val; }
  int InputDescriptor_GetSubifd(InputDescriptor* input) { return input->subifd; }
  void InputDescriptor_SetSubifd(InputDescriptor* input, int val) { input->subifd = val; }
  int InputDescriptor_GetCreateChannels(InputDescriptor* input) { return input->createChannels; }
  void InputDescriptor_SetCreateChannels(InputDescriptor* input, int val) { input->createChannels = val; }
  int InputDescriptor_GetCreateWidth(InputDescriptor* input) { return input->createWidth; }
  void InputDescriptor_SetCreateWidth(InputDescriptor* input, int val) { input->createWidth = val; }
  int InputDescriptor_GetCreateHeight(InputDescriptor* input) { return input->createHeight; }
  void InputDescriptor_SetCreateHeight(InputDescriptor* input, int val) { input->createHeight = val; }
  double* InputDescriptor_GetCreateBackground(InputDescriptor* input) { return input->createBackground.data(); }
  void InputDescriptor_SetCreateBackground(InputDescriptor* input, double* val, size_t count) { input->createBackground = std::vector<double>(val, val + count); }
  const char* InputDescriptor_GetCreateNoiseType(InputDescriptor* input) { return input->createNoiseType.c_str(); }
  void InputDescriptor_SetCreateNoiseType(InputDescriptor* input, const char* val) { input->createNoiseType = val; }
  double InputDescriptor_GetCreateNoiseMean(InputDescriptor* input) { return input->createNoiseMean; }
  void InputDescriptor_SetCreateNoiseMean(InputDescriptor* input, double val) { input->createNoiseMean = val; }
  double InputDescriptor_GetCreateNoiseSigma(InputDescriptor* input) { return input->createNoiseSigma; }
  void InputDescriptor_SetCreateNoiseSigma(InputDescriptor* input, double val) { input->createNoiseSigma = val; }
}

namespace sharp {
  // Filename extension checkers
  static bool EndsWith(std::string const &str, std::string const &end) {
    return str.length() >= end.length() && 0 == str.compare(str.length() - end.length(), end.length(), end);
  }
  bool IsJpeg(std::string const &str) {
    return EndsWith(str, ".jpg") || EndsWith(str, ".jpeg") || EndsWith(str, ".JPG") || EndsWith(str, ".JPEG");
  }
  bool IsPng(std::string const &str) {
    return EndsWith(str, ".png") || EndsWith(str, ".PNG");
  }
  bool IsWebp(std::string const &str) {
    return EndsWith(str, ".webp") || EndsWith(str, ".WEBP");
  }
  bool IsGif(std::string const &str) {
    return EndsWith(str, ".gif") || EndsWith(str, ".GIF");
  }
  bool IsJp2(std::string const &str) {
    return EndsWith(str, ".jp2") || EndsWith(str, ".jpx") || EndsWith(str, ".j2k") || EndsWith(str, ".j2c")
      || EndsWith(str, ".JP2") || EndsWith(str, ".JPX") || EndsWith(str, ".J2K") || EndsWith(str, ".J2C");
  }
  bool IsTiff(std::string const &str) {
    return EndsWith(str, ".tif") || EndsWith(str, ".tiff") || EndsWith(str, ".TIF") || EndsWith(str, ".TIFF");
  }
  bool IsHeic(std::string const &str) {
    return EndsWith(str, ".heic") || EndsWith(str, ".HEIC");
  }
  bool IsHeif(std::string const &str) {
    return EndsWith(str, ".heif") || EndsWith(str, ".HEIF") || IsHeic(str) || IsAvif(str);
  }
  bool IsAvif(std::string const &str) {
    return EndsWith(str, ".avif") || EndsWith(str, ".AVIF");
  }
  bool IsDz(std::string const &str) {
    return EndsWith(str, ".dzi") || EndsWith(str, ".DZI");
  }
  bool IsDzZip(std::string const &str) {
    return EndsWith(str, ".zip") || EndsWith(str, ".ZIP") || EndsWith(str, ".szi") || EndsWith(str, ".SZI");
  }
  bool IsV(std::string const &str) {
    return EndsWith(str, ".v") || EndsWith(str, ".V") || EndsWith(str, ".vips") || EndsWith(str, ".VIPS");
  }

  /*
    Provide a string identifier for the given image type.
  */
  std::string ImageTypeId(ImageType const imageType) {
    std::string id;
    switch (imageType) {
      case ImageType::JPEG: id = "jpeg"; break;
      case ImageType::PNG: id = "png"; break;
      case ImageType::WEBP: id = "webp"; break;
      case ImageType::TIFF: id = "tiff"; break;
      case ImageType::GIF: id = "gif"; break;
      case ImageType::JP2: id = "jp2"; break;
      case ImageType::SVG: id = "svg"; break;
      case ImageType::HEIF: id = "heif"; break;
      case ImageType::PDF: id = "pdf"; break;
      case ImageType::MAGICK: id = "magick"; break;
      case ImageType::OPENSLIDE: id = "openslide"; break;
      case ImageType::PPM: id = "ppm"; break;
      case ImageType::FITS: id = "fits"; break;
      case ImageType::EXR: id = "exr"; break;
      case ImageType::VIPS: id = "vips"; break;
      case ImageType::RAW: id = "raw"; break;
      case ImageType::UNKNOWN: id = "unknown"; break;
      case ImageType::MISSING: id = "missing"; break;
    }
    return id;
  }

  /**
   * Regenerate this table with something like:
   *
   * $ vips -l foreign | grep -i load | awk '{ print $2, $1; }'
   *
   * Plus a bit of editing.
   */
  std::map<std::string, ImageType> loaderToType = {
    { "VipsForeignLoadJpegFile", ImageType::JPEG },
    { "VipsForeignLoadJpegBuffer", ImageType::JPEG },
    { "VipsForeignLoadPngFile", ImageType::PNG },
    { "VipsForeignLoadPngBuffer", ImageType::PNG },
    { "VipsForeignLoadWebpFile", ImageType::WEBP },
    { "VipsForeignLoadWebpBuffer", ImageType::WEBP },
    { "VipsForeignLoadTiffFile", ImageType::TIFF },
    { "VipsForeignLoadTiffBuffer", ImageType::TIFF },
    { "VipsForeignLoadGifFile", ImageType::GIF },
    { "VipsForeignLoadGifBuffer", ImageType::GIF },
    { "VipsForeignLoadNsgifFile", ImageType::GIF },
    { "VipsForeignLoadNsgifBuffer", ImageType::GIF },
    { "VipsForeignLoadJp2kBuffer", ImageType::JP2 },
    { "VipsForeignLoadJp2kFile", ImageType::JP2 },
    { "VipsForeignLoadSvgFile", ImageType::SVG },
    { "VipsForeignLoadSvgBuffer", ImageType::SVG },
    { "VipsForeignLoadHeifFile", ImageType::HEIF },
    { "VipsForeignLoadHeifBuffer", ImageType::HEIF },
    { "VipsForeignLoadPdfFile", ImageType::PDF },
    { "VipsForeignLoadPdfBuffer", ImageType::PDF },
    { "VipsForeignLoadMagickFile", ImageType::MAGICK },
    { "VipsForeignLoadMagickBuffer", ImageType::MAGICK },
    { "VipsForeignLoadMagick7File", ImageType::MAGICK },
    { "VipsForeignLoadMagick7Buffer", ImageType::MAGICK },
    { "VipsForeignLoadOpenslide", ImageType::OPENSLIDE },
    { "VipsForeignLoadPpmFile", ImageType::PPM },
    { "VipsForeignLoadFits", ImageType::FITS },
    { "VipsForeignLoadOpenexr", ImageType::EXR },
    { "VipsForeignLoadVips", ImageType::VIPS },
    { "VipsForeignLoadVipsFile", ImageType::VIPS },
    { "VipsForeignLoadRaw", ImageType::RAW }
  };

  /*
    Determine image format of a buffer.
  */
  ImageType DetermineImageType(void *buffer, size_t const length) {
    ImageType imageType = ImageType::UNKNOWN;
    char const *load = vips_foreign_find_load_buffer(buffer, length);
    if (load != nullptr) {
      auto it = loaderToType.find(load);
      if (it != loaderToType.end()) {
        imageType = it->second;
      }
    }
    return imageType;
  }

  /*
    Determine image format, reads the first few bytes of the file
  */
  ImageType DetermineImageType(char const *file) {
    ImageType imageType = ImageType::UNKNOWN;
    char const *load = vips_foreign_find_load(file);
    if (load != nullptr) {
      auto it = loaderToType.find(load);
      if (it != loaderToType.end()) {
        imageType = it->second;
      }
    } else {
      if (EndsWith(vips::VError().what(), " does not exist\n")) {
        imageType = ImageType::MISSING;
      }
    }
    return imageType;
  }

  /*
    Does this image type support multiple pages?
  */
  bool ImageTypeSupportsPage(ImageType imageType) {
    return
      imageType == ImageType::WEBP ||
      imageType == ImageType::MAGICK ||
      imageType == ImageType::GIF ||
      imageType == ImageType::JP2 ||
      imageType == ImageType::TIFF ||
      imageType == ImageType::HEIF ||
      imageType == ImageType::PDF;
  }

  /*
    Open an image from the given InputDescriptor (filesystem, compressed buffer, raw pixel data)
  */
  std::tuple<VImage, ImageType> OpenInput(InputDescriptor *descriptor) {
    VImage image;
    ImageType imageType;
    if (descriptor->isBuffer) {
      if (descriptor->rawChannels > 0) {
        // Raw, uncompressed pixel data
        image = VImage::new_from_memory(descriptor->buffer, descriptor->bufferLength,
          descriptor->rawWidth, descriptor->rawHeight, descriptor->rawChannels, descriptor->rawDepth);
        if (descriptor->rawChannels < 3) {
          image.get_image()->Type = VIPS_INTERPRETATION_B_W;
        } else {
          image.get_image()->Type = VIPS_INTERPRETATION_sRGB;
        }
        if (descriptor->rawPremultiplied) {
          image = image.unpremultiply();
        }
        imageType = ImageType::RAW;
      } else {
        // Compressed data
        imageType = DetermineImageType(descriptor->buffer, descriptor->bufferLength);
        if (imageType != ImageType::UNKNOWN) {
          try {
            vips::VOption *option = VImage::option()
              ->set("access", descriptor->access)
              ->set("fail", descriptor->failOnError);
            if (descriptor->unlimited && (imageType == ImageType::SVG || imageType == ImageType::PNG)) {
              option->set("unlimited", TRUE);
            }
            if (imageType == ImageType::SVG || imageType == ImageType::PDF) {
              option->set("dpi", descriptor->density);
            }
            if (imageType == ImageType::MAGICK) {
              option->set("density", std::to_string(descriptor->density).data());
            }
            if (ImageTypeSupportsPage(imageType)) {
              option->set("n", descriptor->pages);
              option->set("page", descriptor->page);
            }
            if (imageType == ImageType::OPENSLIDE) {
              option->set("level", descriptor->level);
            }
            if (imageType == ImageType::TIFF) {
              option->set("subifd", descriptor->subifd);
            }
            image = VImage::new_from_buffer(descriptor->buffer, descriptor->bufferLength, nullptr, option);
            if (imageType == ImageType::SVG || imageType == ImageType::PDF || imageType == ImageType::MAGICK) {
              image = SetDensity(image, descriptor->density);
            }
          } catch (vips::VError const &err) {
            throw vips::VError(std::string("Input buffer has corrupt header: ") + err.what());
          }
        } else {
          throw vips::VError("Input buffer contains unsupported image format");
        }
      }
    } else {
      if (descriptor->createChannels > 0) {
        // Create new image
        if (descriptor->createNoiseType == "gaussian") {
          int const channels = descriptor->createChannels;
          image = VImage::new_matrix(descriptor->createWidth, descriptor->createHeight);
          std::vector<VImage> bands = {};
          bands.reserve(channels);
          for (int _band = 0; _band < channels; _band++) {
            bands.push_back(image.gaussnoise(
              descriptor->createWidth,
              descriptor->createHeight,
              VImage::option()->set("mean", descriptor->createNoiseMean)->set("sigma", descriptor->createNoiseSigma)));
          }
          image = image.bandjoin(bands);
          image = image.cast(VipsBandFormat::VIPS_FORMAT_UCHAR);
          if (channels < 3) {
            image = image.colourspace(VIPS_INTERPRETATION_B_W);
          } else {
            image = image.colourspace(VIPS_INTERPRETATION_sRGB);
          }
        } else {
          std::vector<double> background = {
            descriptor->createBackground[0],
            descriptor->createBackground[1],
            descriptor->createBackground[2]
          };
          if (descriptor->createChannels == 4) {
            background.push_back(descriptor->createBackground[3]);
          }
          image = VImage::new_matrix(descriptor->createWidth, descriptor->createHeight).new_from_image(background);
        }
        image.get_image()->Type = VIPS_INTERPRETATION_sRGB;
        imageType = ImageType::RAW;
      } else {
        // From filesystem
        imageType = DetermineImageType(descriptor->file.data());
        if (imageType == ImageType::MISSING) {
          if (descriptor->file.find("<svg") != std::string::npos) {
            throw vips::VError("Input file is missing, did you mean "
              "sharp(Buffer.from('" + descriptor->file.substr(0, 8) + "...')?");
          }
          throw vips::VError("Input file is missing");
        }
        if (imageType != ImageType::UNKNOWN) {
          try {
            vips::VOption *option = VImage::option()
              ->set("access", descriptor->access)
              ->set("fail", descriptor->failOnError);
            if (descriptor->unlimited && (imageType == ImageType::SVG || imageType == ImageType::PNG)) {
              option->set("unlimited", TRUE);
            }
            if (imageType == ImageType::SVG || imageType == ImageType::PDF) {
              option->set("dpi", descriptor->density);
            }
            if (imageType == ImageType::MAGICK) {
              option->set("density", std::to_string(descriptor->density).data());
            }
            if (ImageTypeSupportsPage(imageType)) {
              option->set("n", descriptor->pages);
              option->set("page", descriptor->page);
            }
            if (imageType == ImageType::OPENSLIDE) {
              option->set("level", descriptor->level);
            }
            if (imageType == ImageType::TIFF) {
              option->set("subifd", descriptor->subifd);
            }
            image = VImage::new_from_file(descriptor->file.data(), option);
            if (imageType == ImageType::SVG || imageType == ImageType::PDF || imageType == ImageType::MAGICK) {
              image = SetDensity(image, descriptor->density);
            }
          } catch (vips::VError const &err) {
            throw vips::VError(std::string("Input file has corrupt header: ") + err.what());
          }
        } else {
          throw vips::VError("Input file contains unsupported image format");
        }
      }
    }
    // Limit input images to a given number of pixels, where pixels = width * height
    if (descriptor->limitInputPixels > 0 &&
      static_cast<uint64_t>(image.width() * image.height()) > static_cast<uint64_t>(descriptor->limitInputPixels)) {
      throw vips::VError("Input image exceeds pixel limit");
    }
    return std::make_tuple(image, imageType);
  }

  /*
    Does this image have an embedded profile?
  */
  bool HasProfile(VImage image) {
    return (image.get_typeof(VIPS_META_ICC_NAME) != 0) ? TRUE : FALSE;
  }

  /*
    Does this image have an alpha channel?
    Uses colour space interpretation with number of channels to guess this.
  */
  bool HasAlpha(VImage image) {
    return image.has_alpha();
  }

  /*
    Get EXIF Orientation of image, if any.
  */
  int ExifOrientation(VImage image) {
    int orientation = 0;
    if (image.get_typeof(VIPS_META_ORIENTATION) != 0) {
      orientation = image.get_int(VIPS_META_ORIENTATION);
    }
    return orientation;
  }

  /*
    Set EXIF Orientation of image.
  */
  VImage SetExifOrientation(VImage image, int const orientation) {
    VImage copy = image.copy();
    copy.set(VIPS_META_ORIENTATION, orientation);
    return copy;
  }

  /*
    Remove EXIF Orientation from image.
  */
  VImage RemoveExifOrientation(VImage image) {
    VImage copy = image.copy();
    copy.remove(VIPS_META_ORIENTATION);
    return copy;
  }

  /*
    Set animation properties if necessary.
  */
  VImage SetAnimationProperties(VImage image, int nPages, int pageHeight, std::vector<int> delay, int loop) {
    bool hasDelay = !delay.empty();

    // Avoid a copy if none of the animation properties are needed.
    if (nPages == 1 && !hasDelay && loop == -1) return image;

    if (delay.size() == 1) {
      // We have just one delay, repeat that value for all frames.
      delay.insert(delay.end(), nPages - 1, delay[0]);
    }

    // Attaching metadata, need to copy the image.
    VImage copy = image.copy();

    // Only set page-height if we have more than one page, or this could
    // accidentally turn into an animated image later.
    if (nPages > 1) copy.set(VIPS_META_PAGE_HEIGHT, pageHeight);
    if (hasDelay) copy.set("delay", delay);
    if (loop != -1) copy.set("loop", loop);

    return copy;
  }

  /*
    Remove animation properties from image.
  */
  VImage RemoveAnimationProperties(VImage image) {
    VImage copy = image.copy();
    copy.remove(VIPS_META_PAGE_HEIGHT);
    copy.remove("delay");
    copy.remove("loop");
    return copy;
  }

  /*
    Does this image have a non-default density?
  */
  bool HasDensity(VImage image) {
    return image.xres() > 1.0;
  }

  /*
    Get pixels/mm resolution as pixels/inch density.
  */
  int GetDensity(VImage image) {
    return static_cast<int>(round(image.xres() * 25.4));
  }

  /*
    Set pixels/mm resolution based on a pixels/inch density.
  */
  VImage SetDensity(VImage image, const double density) {
    const double pixelsPerMm = density / 25.4;
    VImage copy = image.copy();
    copy.get_image()->Xres = pixelsPerMm;
    copy.get_image()->Yres = pixelsPerMm;
    return copy;
  }

  /*
    Multi-page images can have a page height. Fetch it, and sanity check it.
    If page-height is not set, it defaults to the image height
  */
  int GetPageHeight(VImage image) {
    return vips_image_get_page_height(image.get_image());
  }

  /*
    Check the proposed format supports the current dimensions.
  */
  void AssertImageTypeDimensions(VImage image, ImageType const imageType) {
    const int height = image.get_typeof(VIPS_META_PAGE_HEIGHT) == G_TYPE_INT
      ? image.get_int(VIPS_META_PAGE_HEIGHT)
      : image.height();
    if (imageType == ImageType::JPEG) {
      if (image.width() > 65535 || height > 65535) {
        throw vips::VError("Processed image is too large for the JPEG format");
      }
    } else if (imageType == ImageType::WEBP) {
      if (image.width() > 16383 || height > 16383) {
        throw vips::VError("Processed image is too large for the WebP format");
      }
    } else if (imageType == ImageType::GIF) {
      if (image.width() > 65535 || height > 65535) {
        throw vips::VError("Processed image is too large for the GIF format");
      }
    }
  }

  /*
    Attach an event listener for progress updates, used to detect timeout
  */
  void SetTimeout(VImage image, int const seconds) {
    if (seconds > 0) {
      VipsImage *im = image.get_image();
      if (im->progress_signal == NULL) {
        int *timeout = VIPS_NEW(im, int);
        *timeout = seconds;
        g_signal_connect(im, "eval", G_CALLBACK(VipsProgressCallBack), timeout);
        vips_image_set_progress(im, TRUE);
      }
    }
  }

  /*
    Event listener for progress updates, used to detect timeout
  */
  void VipsProgressCallBack(VipsImage *im, VipsProgress *progress, int *timeout) {
    // printf("VipsProgressCallBack progress=%d run=%d timeout=%d\n", progress->percent, progress->run, *timeout);
    if (*timeout > 0 && progress->run >= *timeout) {
      vips_image_set_kill(im, TRUE);
      vips_error("timeout", "%d%% complete", progress->percent);
      *timeout = 0;
    }
  }

  /*
    Calculate the (left, top) coordinates of the output image
    within the input image, applying the given gravity during an embed.

    @Azurebyte: We are basically swapping the inWidth and outWidth, inHeight and outHeight from the CalculateCrop function.
  */
  std::tuple<int, int> CalculateEmbedPosition(int const inWidth, int const inHeight,
    int const outWidth, int const outHeight, int const gravity) {

    int left = 0;
    int top = 0;
    switch (gravity) {
      case 1:
        // North
        left = (outWidth - inWidth) / 2;
        break;
      case 2:
        // East
        left = outWidth - inWidth;
        top = (outHeight - inHeight) / 2;
        break;
      case 3:
        // South
        left = (outWidth - inWidth) / 2;
        top = outHeight - inHeight;
        break;
      case 4:
        // West
        top = (outHeight - inHeight) / 2;
        break;
      case 5:
        // Northeast
        left = outWidth - inWidth;
        break;
      case 6:
        // Southeast
        left = outWidth - inWidth;
        top = outHeight - inHeight;
        break;
      case 7:
        // Southwest
        top = outHeight - inHeight;
        break;
      case 8:
        // Northwest
        // Which is the default is 0,0 so we do not assign anything here.
        break;
      default:
        // Centre
        left = (outWidth - inWidth) / 2;
        top = (outHeight - inHeight) / 2;
    }
    return std::make_tuple(left, top);
  }

  /*
    Calculate the (left, top) coordinates of the output image
    within the input image, applying the given gravity during a crop.
  */
  std::tuple<int, int> CalculateCrop(int const inWidth, int const inHeight,
    int const outWidth, int const outHeight, int const gravity) {

    int left = 0;
    int top = 0;
    switch (gravity) {
      case 1:
        // North
        left = (inWidth - outWidth + 1) / 2;
        break;
      case 2:
        // East
        left = inWidth - outWidth;
        top = (inHeight - outHeight + 1) / 2;
        break;
      case 3:
        // South
        left = (inWidth - outWidth + 1) / 2;
        top = inHeight - outHeight;
        break;
      case 4:
        // West
        top = (inHeight - outHeight + 1) / 2;
        break;
      case 5:
        // Northeast
        left = inWidth - outWidth;
        break;
      case 6:
        // Southeast
        left = inWidth - outWidth;
        top = inHeight - outHeight;
        break;
      case 7:
        // Southwest
        top = inHeight - outHeight;
        break;
      case 8:
        // Northwest
        break;
      default:
        // Centre
        left = (inWidth - outWidth + 1) / 2;
        top = (inHeight - outHeight + 1) / 2;
    }
    return std::make_tuple(left, top);
  }

  /*
    Calculate the (left, top) coordinates of the output image
    within the input image, applying the given x and y offsets.
  */
  std::tuple<int, int> CalculateCrop(int const inWidth, int const inHeight,
    int const outWidth, int const outHeight, int const x, int const y) {

    // default values
    int left = 0;
    int top = 0;

    // assign only if valid
    if (x < (inWidth - outWidth)) {
      left = x;
    } else if (x >= (inWidth - outWidth)) {
      left = inWidth - outWidth;
    }

    if (y < (inHeight - outHeight)) {
      top = y;
    } else if (y >= (inHeight - outHeight)) {
      top = inHeight - outHeight;
    }

    return std::make_tuple(left, top);
  }

  /*
    Are pixel values in this image 16-bit integer?
  */
  bool Is16Bit(VipsInterpretation const interpretation) {
    return interpretation == VIPS_INTERPRETATION_RGB16 || interpretation == VIPS_INTERPRETATION_GREY16;
  }

  /*
    Return the image alpha maximum. Useful for combining alpha bands. scRGB
    images are 0 - 1 for image data, but the alpha is 0 - 255.
  */
  double MaximumImageAlpha(VipsInterpretation const interpretation) {
    return Is16Bit(interpretation) ? 65535.0 : 255.0;
  }

  /*
    Get boolean operation type from string
  */
  VipsOperationBoolean GetBooleanOperation(std::string const opStr) {
    return static_cast<VipsOperationBoolean>(
      vips_enum_from_nick(nullptr, VIPS_TYPE_OPERATION_BOOLEAN, opStr.data()));
  }

  /*
    Get interpretation type from string
  */
  VipsInterpretation GetInterpretation(std::string const typeStr) {
    return static_cast<VipsInterpretation>(
      vips_enum_from_nick(nullptr, VIPS_TYPE_INTERPRETATION, typeStr.data()));
  }

  /*
    Convert RGBA value to another colourspace
  */
  std::vector<double> GetRgbaAsColourspace(std::vector<double> const rgba,
    VipsInterpretation const interpretation, bool premultiply) {
    int const bands = static_cast<int>(rgba.size());
    if (bands < 3) {
      return rgba;
    }
    VImage pixel = VImage::new_matrix(1, 1);
    pixel.set("bands", bands);
    pixel = pixel
      .new_from_image(rgba)
      .colourspace(interpretation, VImage::option()->set("source_space", VIPS_INTERPRETATION_sRGB));
    if (premultiply) {
      pixel = pixel.premultiply();
    }
    return pixel(0, 0);
  }

  /*
    Apply the alpha channel to a given colour
  */
  std::tuple<VImage, std::vector<double>> ApplyAlpha(VImage image, std::vector<double> colour, bool premultiply) {
    // Scale up 8-bit values to match 16-bit input image
    double const multiplier = sharp::Is16Bit(image.interpretation()) ? 256.0 : 1.0;
    // Create alphaColour colour
    std::vector<double> alphaColour;
    if (image.bands() > 2) {
      alphaColour = {
        multiplier * colour[0],
        multiplier * colour[1],
        multiplier * colour[2]
      };
    } else {
      // Convert sRGB to greyscale
      alphaColour = { multiplier * (
        0.2126 * colour[0] +
        0.7152 * colour[1] +
        0.0722 * colour[2])
      };
    }
    // Add alpha channel to alphaColour colour
    if (colour[3] < 255.0 || HasAlpha(image)) {
      alphaColour.push_back(colour[3] * multiplier);
    }
    // Ensure alphaColour colour uses correct colourspace
    alphaColour = sharp::GetRgbaAsColourspace(alphaColour, image.interpretation(), premultiply);
    // Add non-transparent alpha channel, if required
    if (colour[3] < 255.0 && !HasAlpha(image)) {
      image = image.bandjoin(
        VImage::new_matrix(image.width(), image.height()).new_from_image(255 * multiplier));
    }
    return std::make_tuple(image, alphaColour);
  }

  /*
    Removes alpha channel, if any.
  */
  VImage RemoveAlpha(VImage image) {
    if (HasAlpha(image)) {
      image = image.extract_band(0, VImage::option()->set("n", image.bands() - 1));
    }
    return image;
  }

  /*
    Ensures alpha channel, if missing.
  */
  VImage EnsureAlpha(VImage image, double const value) {
    if (!HasAlpha(image)) {
      std::vector<double> alpha;
      alpha.push_back(value * sharp::MaximumImageAlpha(image.interpretation()));
      image = image.bandjoin_const(alpha);
    }
    return image;
  }

  std::pair<double, double> ResolveShrink(int width, int height, int targetWidth, int targetHeight,
    Canvas canvas, bool swap, bool withoutEnlargement, bool withoutReduction) {
    if (swap) {
      // Swap input width and height when requested.
      std::swap(width, height);
    }

    double hshrink = 1.0;
    double vshrink = 1.0;

    if (targetWidth > 0 && targetHeight > 0) {
      // Fixed width and height
      hshrink = static_cast<double>(width) / targetWidth;
      vshrink = static_cast<double>(height) / targetHeight;

      switch (canvas) {
        case Canvas::CROP:
        case Canvas::MIN:
          if (hshrink < vshrink) {
            vshrink = hshrink;
          } else {
            hshrink = vshrink;
          }
          break;
        case Canvas::EMBED:
        case Canvas::MAX:
          if (hshrink > vshrink) {
            vshrink = hshrink;
          } else {
            hshrink = vshrink;
          }
          break;
        case Canvas::IGNORE_ASPECT:
          if (swap) {
            std::swap(hshrink, vshrink);
          }
          break;
      }
    } else if (targetWidth > 0) {
      // Fixed width
      hshrink = static_cast<double>(width) / targetWidth;

      if (canvas != Canvas::IGNORE_ASPECT) {
        // Auto height
        vshrink = hshrink;
      }
    } else if (targetHeight > 0) {
      // Fixed height
      vshrink = static_cast<double>(height) / targetHeight;

      if (canvas != Canvas::IGNORE_ASPECT) {
        // Auto width
        hshrink = vshrink;
      }
    }

    // We should not reduce or enlarge the output image, if
    // withoutReduction or withoutEnlargement is specified.
    if (withoutReduction) {
      // Equivalent of VIPS_SIZE_UP
      hshrink = std::min(1.0, hshrink);
      vshrink = std::min(1.0, vshrink);
    } else if (withoutEnlargement) {
      // Equivalent of VIPS_SIZE_DOWN
      hshrink = std::max(1.0, hshrink);
      vshrink = std::max(1.0, vshrink);
    }

    // We don't want to shrink so much that we send an axis to 0
    hshrink = std::min(hshrink, static_cast<double>(width));
    vshrink = std::min(vshrink, static_cast<double>(height));

    return std::make_pair(hshrink, vshrink);
  }

}  // namespace sharp
