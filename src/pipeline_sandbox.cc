
#include <vips/vips8>

#include "common_sandbox.h"
#include "operations.h"
#include "pipeline_sandbox.h"

static void MultiPageUnsupported(int const pages, std::string op) {
  if (pages > 1) {
    throw vips::VError(op + " is not supported for multi-page images");
  }
}

/*
  Calculate the angle of rotation and need-to-flip for the given Exif orientation
  By default, returns zero, i.e. no rotation.
*/
static std::tuple<VipsAngle, bool, bool>
CalculateExifRotationAndFlip(int const exifOrientation) {
  VipsAngle rotate = VIPS_ANGLE_D0;
  bool flip = FALSE;
  bool flop = FALSE;
  switch (exifOrientation) {
    case 6: rotate = VIPS_ANGLE_D90; break;
    case 3: rotate = VIPS_ANGLE_D180; break;
    case 8: rotate = VIPS_ANGLE_D270; break;
    case 2: flop = TRUE; break;  // flop 1
    case 7: flip = TRUE; rotate = VIPS_ANGLE_D90; break;  // flip 6
    case 4: flop = TRUE; rotate = VIPS_ANGLE_D180; break;  // flop 3
    case 5: flip = TRUE; rotate = VIPS_ANGLE_D270; break;  // flip 8
  }
  return std::make_tuple(rotate, flip, flop);
}


/*
  Calculate the rotation for the given angle.
  Supports any positive or negative angle that is a multiple of 90.
*/
static VipsAngle
CalculateAngleRotation(int angle) {
  angle = angle % 360;
  if (angle < 0)
    angle = 360 + angle;
  switch (angle) {
    case 90: return VIPS_ANGLE_D90;
    case 180: return VIPS_ANGLE_D180;
    case 270: return VIPS_ANGLE_D270;
  }
  return VIPS_ANGLE_D0;
}


/*
  Assemble the suffix argument to dzsave, which is the format (by extname)
  alongisde comma-separated arguments to the corresponding `formatsave` vips
  action.
*/
static std::string
AssembleSuffixString(std::string extname, std::vector<std::pair<std::string, std::string>> options) {
  std::string argument;
  for (auto const &option : options) {
    if (!argument.empty()) {
      argument += ",";
    }
    argument += option.first + "=" + option.second;
  }
  return extname + "[" + argument + "]";
}

/*
  Clear all thread-local data.
*/
static void Error() {
  // Clean up libvips' per-request data and threads
  vips_error_clear();
  vips_thread_shutdown();
}


void PipelineWorkerExecute(PipelineBaton *baton) {

  try {
    // Open input
    vips::VImage image;
    sharp::ImageType inputImageType;
    std::tie(image, inputImageType) = sharp::OpenInput(baton->input);
    image = sharp::EnsureColourspace(image, baton->colourspaceInput);

    int nPages = baton->input->pages;
    if (nPages == -1) {
      // Resolve the number of pages if we need to render until the end of the document
      nPages = image.get_typeof(VIPS_META_N_PAGES) != 0
        ? image.get_int(VIPS_META_N_PAGES) - baton->input->page
        : 1;
    }

    // Get pre-resize page height
    int pageHeight = sharp::GetPageHeight(image);

    // Calculate angle of rotation
    VipsAngle rotation;
    bool flip = FALSE;
    bool flop = FALSE;
    if (baton->useExifOrientation) {
      // Rotate and flip image according to Exif orientation
      std::tie(rotation, flip, flop) = CalculateExifRotationAndFlip(sharp::ExifOrientation(image));
    } else {
      rotation = CalculateAngleRotation(baton->angle);
    }

    // Rotate pre-extract
    if (baton->rotateBeforePreExtract) {
      if (rotation != VIPS_ANGLE_D0) {
        image = image.rot(rotation);
        if (flip) {
          image = image.flip(VIPS_DIRECTION_VERTICAL);
          flip = FALSE;
        }
        if (flop) {
          image = image.flip(VIPS_DIRECTION_HORIZONTAL);
          flop = FALSE;
        }
        image = sharp::RemoveExifOrientation(image);
      }
      if (baton->rotationAngle != 0.0) {
        MultiPageUnsupported(nPages, "Rotate");
        std::vector<double> background;
        std::tie(image, background) = sharp::ApplyAlpha(image, baton->rotationBackground, FALSE);
        image = image.rotate(baton->rotationAngle, VImage::option()->set("background", background));
      }
    }

    // Trim
    if (baton->trimThreshold > 0.0) {
      MultiPageUnsupported(nPages, "Trim");
      image = sharp::Trim(image, baton->trimThreshold);
      baton->trimOffsetLeft = image.xoffset();
      baton->trimOffsetTop = image.yoffset();
    }

    // Pre extraction
    if (baton->topOffsetPre != -1) {
      image = nPages > 1
        ? sharp::CropMultiPage(image,
            baton->leftOffsetPre, baton->topOffsetPre, baton->widthPre, baton->heightPre, nPages, &pageHeight)
        : image.extract_area(baton->leftOffsetPre, baton->topOffsetPre, baton->widthPre, baton->heightPre);
    }

    // Get pre-resize image width and height
    int inputWidth = image.width();
    int inputHeight = image.height();

    // Is there just one page? Shrink to inputHeight instead
    if (nPages == 1) {
      pageHeight = inputHeight;
    }

    // Scaling calculations
    double hshrink;
    double vshrink;
    int targetResizeWidth = baton->width;
    int targetResizeHeight = baton->height;

    // Swap input output width and height when rotating by 90 or 270 degrees
    bool swap = !baton->rotateBeforePreExtract && (rotation == VIPS_ANGLE_D90 || rotation == VIPS_ANGLE_D270);

    // Shrink to pageHeight, so we work for multi-page images
    std::tie(hshrink, vshrink) = sharp::ResolveShrink(
      inputWidth, pageHeight, targetResizeWidth, targetResizeHeight,
      baton->canvas, swap, baton->withoutEnlargement, baton->withoutReduction);

    // The jpeg preload shrink.
    int jpegShrinkOnLoad = 1;

    // WebP, PDF, SVG scale
    double scale = 1.0;

    // Try to reload input using shrink-on-load for JPEG, WebP, SVG and PDF, when:
    //  - the width or height parameters are specified;
    //  - gamma correction doesn't need to be applied;
    //  - trimming or pre-resize extract isn't required;
    //  - input colourspace is not specified;
    bool const shouldPreShrink = (targetResizeWidth > 0 || targetResizeHeight > 0) &&
      baton->gamma == 0 && baton->topOffsetPre == -1 && baton->trimThreshold == 0.0 &&
      baton->colourspaceInput == VIPS_INTERPRETATION_LAST;

    if (shouldPreShrink) {
      // The common part of the shrink: the bit by which both axes must be shrunk
      double shrink = std::min(hshrink, vshrink);

      if (inputImageType == sharp::ImageType::JPEG) {
        // Leave at least a factor of two for the final resize step, when fastShrinkOnLoad: false
        // for more consistent results and to avoid extra sharpness to the image
        int factor = baton->fastShrinkOnLoad ? 1 : 2;
        if (shrink >= 8 * factor) {
          jpegShrinkOnLoad = 8;
        } else if (shrink >= 4 * factor) {
          jpegShrinkOnLoad = 4;
        } else if (shrink >= 2 * factor) {
          jpegShrinkOnLoad = 2;
        }
        // Lower shrink-on-load for known libjpeg rounding errors
        if (jpegShrinkOnLoad > 1 && static_cast<int>(shrink) == jpegShrinkOnLoad) {
          jpegShrinkOnLoad /= 2;
        }
      } else if (inputImageType == sharp::ImageType::WEBP ||
                 inputImageType == sharp::ImageType::SVG ||
                 inputImageType == sharp::ImageType::PDF) {
        scale = 1.0 / shrink;
      }
    }

    // Reload input using shrink-on-load, it'll be an integer shrink
    // factor for jpegload*, a double scale factor for webpload*,
    // pdfload* and svgload*
    if (jpegShrinkOnLoad > 1) {
      vips::VOption *option = VImage::option()
        ->set("access", baton->input->access)
        ->set("shrink", jpegShrinkOnLoad)
        ->set("fail", baton->input->failOnError);
      if (baton->input->buffer != nullptr) {
        // Reload JPEG buffer
        VipsBlob *blob = vips_blob_new(nullptr, baton->input->buffer, baton->input->bufferLength);
        image = VImage::jpegload_buffer(blob, option);
        vips_area_unref(reinterpret_cast<VipsArea*>(blob));
      } else {
        // Reload JPEG file
        image = VImage::jpegload(const_cast<char*>(baton->input->file.data()), option);
      }
    } else if (scale != 1.0) {
      vips::VOption *option = VImage::option()
        ->set("access", baton->input->access)
        ->set("scale", scale)
        ->set("fail", baton->input->failOnError);
      if (inputImageType == sharp::ImageType::WEBP) {
        option->set("n", baton->input->pages);
        option->set("page", baton->input->page);

        if (baton->input->buffer != nullptr) {
          // Reload WebP buffer
          VipsBlob *blob = vips_blob_new(nullptr, baton->input->buffer, baton->input->bufferLength);
          image = VImage::webpload_buffer(blob, option);
          vips_area_unref(reinterpret_cast<VipsArea*>(blob));
        } else {
          // Reload WebP file
          image = VImage::webpload(const_cast<char*>(baton->input->file.data()), option);
        }
      } else if (inputImageType == sharp::ImageType::SVG) {
        option->set("unlimited", baton->input->unlimited);
        option->set("dpi", baton->input->density);

        if (baton->input->buffer != nullptr) {
          // Reload SVG buffer
          VipsBlob *blob = vips_blob_new(nullptr, baton->input->buffer, baton->input->bufferLength);
          image = VImage::svgload_buffer(blob, option);
          vips_area_unref(reinterpret_cast<VipsArea*>(blob));
        } else {
          // Reload SVG file
          image = VImage::svgload(const_cast<char*>(baton->input->file.data()), option);
        }

        sharp::SetDensity(image, baton->input->density);
      } else if (inputImageType == sharp::ImageType::PDF) {
        option->set("n", baton->input->pages);
        option->set("page", baton->input->page);
        option->set("dpi", baton->input->density);

        if (baton->input->buffer != nullptr) {
          // Reload PDF buffer
          VipsBlob *blob = vips_blob_new(nullptr, baton->input->buffer, baton->input->bufferLength);
          image = VImage::pdfload_buffer(blob, option);
          vips_area_unref(reinterpret_cast<VipsArea*>(blob));
        } else {
          // Reload PDF file
          image = VImage::pdfload(const_cast<char*>(baton->input->file.data()), option);
        }

        sharp::SetDensity(image, baton->input->density);
      }
    }

    // Any pre-shrinking may already have been done
    inputWidth = image.width();
    inputHeight = image.height();

    // After pre-shrink, but before the main shrink stage
    // Reuse the initial pageHeight if we didn't pre-shrink
    if (shouldPreShrink) {
      pageHeight = sharp::GetPageHeight(image);
    }

    // Shrink to pageHeight, so we work for multi-page images
    std::tie(hshrink, vshrink) = sharp::ResolveShrink(
      inputWidth, pageHeight, targetResizeWidth, targetResizeHeight,
      baton->canvas, swap, baton->withoutEnlargement, baton->withoutReduction);

    int targetHeight = static_cast<int>(std::rint(static_cast<double>(pageHeight) / vshrink));
    int targetPageHeight = targetHeight;

    // In toilet-roll mode, we must adjust vshrink so that we exactly hit
    // pageHeight or we'll have pixels straddling pixel boundaries
    if (inputHeight > pageHeight) {
      targetHeight *= nPages;
      vshrink = static_cast<double>(inputHeight) / targetHeight;
    }

    // Ensure we're using a device-independent colour space
    char const *processingProfile = image.interpretation() == VIPS_INTERPRETATION_RGB16 ? "p3" : "srgb";
    if (
      sharp::HasProfile(image) &&
      image.interpretation() != VIPS_INTERPRETATION_LABS &&
      image.interpretation() != VIPS_INTERPRETATION_GREY16 &&
      image.interpretation() != VIPS_INTERPRETATION_B_W
    ) {
      // Convert to sRGB/P3 using embedded profile
      try {
        image = image.icc_transform(processingProfile, VImage::option()
          ->set("embedded", TRUE)
          ->set("depth", image.interpretation() == VIPS_INTERPRETATION_RGB16 ? 16 : 8)
          ->set("intent", VIPS_INTENT_PERCEPTUAL));
      } catch(...) {
        // Ignore failure of embedded profile
      }
    } else if (image.interpretation() == VIPS_INTERPRETATION_CMYK) {
      image = image.icc_transform(processingProfile, VImage::option()
        ->set("input_profile", "cmyk")
        ->set("intent", VIPS_INTENT_PERCEPTUAL));
    }

    // Flatten image to remove alpha channel
    if (baton->flatten && sharp::HasAlpha(image)) {
      // Scale up 8-bit values to match 16-bit input image
      double const multiplier = sharp::Is16Bit(image.interpretation()) ? 256.0 : 1.0;
      // Background colour
      std::vector<double> background {
        baton->flattenBackground[0] * multiplier,
        baton->flattenBackground[1] * multiplier,
        baton->flattenBackground[2] * multiplier
      };
      image = image.flatten(VImage::option()
        ->set("background", background));
    }

    // Negate the colours in the image
    if (baton->negate) {
      image = sharp::Negate(image, baton->negateAlpha);
    }

    // Gamma encoding (darken)
    if (baton->gamma >= 1 && baton->gamma <= 3) {
      image = sharp::Gamma(image, 1.0 / baton->gamma);
    }

    // Convert to greyscale (linear, therefore after gamma encoding, if any)
    if (baton->greyscale) {
      image = image.colourspace(VIPS_INTERPRETATION_B_W);
    }

    bool const shouldResize = hshrink != 1.0 || vshrink != 1.0;
    bool const shouldBlur = baton->blurSigma != 0.0;
    bool const shouldConv = baton->convKernelWidth * baton->convKernelHeight > 0;
    bool const shouldSharpen = baton->sharpenSigma != 0.0;
    bool const shouldApplyMedian = baton->medianSize > 0;
    bool const shouldComposite = !baton->composite.empty();
    bool const shouldModulate = baton->brightness != 1.0 || baton->saturation != 1.0 ||
                                baton->hue != 0.0 || baton->lightness != 0.0;
    bool const shouldApplyClahe = baton->claheWidth != 0 && baton->claheHeight != 0;

    if (shouldComposite && !sharp::HasAlpha(image)) {
      image = sharp::EnsureAlpha(image, 1);
    }

    bool const shouldPremultiplyAlpha = sharp::HasAlpha(image) &&
      (shouldResize || shouldBlur || shouldConv || shouldSharpen);

    // Premultiply image alpha channel before all transformations to avoid
    // dark fringing around bright pixels
    // See: http://entropymine.com/imageworsener/resizealpha/
    if (shouldPremultiplyAlpha) {
      image = image.premultiply();
    }

    // Resize
    if (shouldResize) {
      VipsKernel kernel = static_cast<VipsKernel>(
        vips_enum_from_nick(nullptr, VIPS_TYPE_KERNEL, baton->kernel.data()));
      if (
        kernel != VIPS_KERNEL_NEAREST && kernel != VIPS_KERNEL_CUBIC && kernel != VIPS_KERNEL_LANCZOS2 &&
        kernel != VIPS_KERNEL_LANCZOS3 && kernel != VIPS_KERNEL_MITCHELL
      ) {
        throw vips::VError("Unknown kernel");
      }
      image = image.resize(1.0 / hshrink, VImage::option()
        ->set("vscale", 1.0 / vshrink)
        ->set("kernel", kernel));
    }

    // Rotate post-extract 90-angle
    if (!baton->rotateBeforePreExtract && rotation != VIPS_ANGLE_D0) {
      image = image.rot(rotation);
      if (flip) {
        image = image.flip(VIPS_DIRECTION_VERTICAL);
        flip = FALSE;
      }
      if (flop) {
        image = image.flip(VIPS_DIRECTION_HORIZONTAL);
        flop = FALSE;
      }
      image = sharp::RemoveExifOrientation(image);
    }

    // Flip (mirror about Y axis)
    if (baton->flip || flip) {
      image = image.flip(VIPS_DIRECTION_VERTICAL);
      image = sharp::RemoveExifOrientation(image);
    }

    // Flop (mirror about X axis)
    if (baton->flop || flop) {
      image = image.flip(VIPS_DIRECTION_HORIZONTAL);
      image = sharp::RemoveExifOrientation(image);
    }

    // Join additional color channels to the image
    if (baton->joinChannelIn.size() > 0) {
      VImage joinImage;
      sharp::ImageType joinImageType = sharp::ImageType::UNKNOWN;

      for (unsigned int i = 0; i < baton->joinChannelIn.size(); i++) {
        std::tie(joinImage, joinImageType) = sharp::OpenInput(baton->joinChannelIn[i]);
        joinImage = sharp::EnsureColourspace(joinImage, baton->colourspaceInput);
        image = image.bandjoin(joinImage);
      }
      image = image.copy(VImage::option()->set("interpretation", baton->colourspace));
    }

    inputWidth = image.width();
    inputHeight = nPages > 1 ? targetPageHeight : image.height();

    // Resolve dimensions
    if (baton->width <= 0) {
      baton->width = inputWidth;
    }
    if (baton->height <= 0) {
      baton->height = inputHeight;
    }

    // Crop/embed
    if (inputWidth != baton->width || inputHeight != baton->height) {
      if (baton->canvas == sharp::Canvas::EMBED) {
        std::vector<double> background;
        std::tie(image, background) = sharp::ApplyAlpha(image, baton->resizeBackground, shouldPremultiplyAlpha);

        // Embed

        // Calculate where to position the embedded image if gravity specified, else center.
        int left;
        int top;

        left = static_cast<int>(round((baton->width - inputWidth) / 2));
        top = static_cast<int>(round((baton->height - inputHeight) / 2));

        int width = std::max(inputWidth, baton->width);
        int height = std::max(inputHeight, baton->height);
        std::tie(left, top) = sharp::CalculateEmbedPosition(
          inputWidth, inputHeight, baton->width, baton->height, baton->position);

        image = nPages > 1
          ? sharp::EmbedMultiPage(image,
              left, top, width, height, background, nPages, &targetPageHeight)
          : image.embed(left, top, width, height, VImage::option()
            ->set("extend", VIPS_EXTEND_BACKGROUND)
            ->set("background", background));
      } else if (baton->canvas == sharp::Canvas::CROP) {
        if (baton->width > inputWidth) {
          baton->width = inputWidth;
        }
        if (baton->height > inputHeight) {
          baton->height = inputHeight;
        }

        // Crop
        if (baton->position < 9) {
          // Gravity-based crop
          int left;
          int top;
          std::tie(left, top) = sharp::CalculateCrop(
            inputWidth, inputHeight, baton->width, baton->height, baton->position);
          int width = std::min(inputWidth, baton->width);
          int height = std::min(inputHeight, baton->height);

          image = nPages > 1
            ? sharp::CropMultiPage(image,
                left, top, width, height, nPages, &targetPageHeight)
            : image.extract_area(left, top, width, height);
        } else {
          // Attention-based or Entropy-based crop
          MultiPageUnsupported(nPages, "Resize strategy");
          image = image.tilecache(VImage::option()
            ->set("access", VIPS_ACCESS_RANDOM)
            ->set("threaded", TRUE));
          image = image.smartcrop(baton->width, baton->height, VImage::option()
            ->set("interesting", baton->position == 16 ? VIPS_INTERESTING_ENTROPY : VIPS_INTERESTING_ATTENTION));
          baton->hasCropOffset = true;
          baton->cropOffsetLeft = static_cast<int>(image.xoffset());
          baton->cropOffsetTop = static_cast<int>(image.yoffset());
        }
      }
    }

    // Rotate post-extract non-90 angle
    if (!baton->rotateBeforePreExtract && baton->rotationAngle != 0.0) {
      MultiPageUnsupported(nPages, "Rotate");
      std::vector<double> background;
      std::tie(image, background) = sharp::ApplyAlpha(image, baton->rotationBackground, shouldPremultiplyAlpha);
      image = image.rotate(baton->rotationAngle, VImage::option()->set("background", background));
    }

    // Post extraction
    if (baton->topOffsetPost != -1) {
      if (nPages > 1) {
        image = sharp::CropMultiPage(image,
          baton->leftOffsetPost, baton->topOffsetPost, baton->widthPost, baton->heightPost,
          nPages, &targetPageHeight);

        // heightPost is used in the info object, so update to reflect the number of pages
        baton->heightPost *= nPages;
      } else {
        image = image.extract_area(
          baton->leftOffsetPost, baton->topOffsetPost, baton->widthPost, baton->heightPost);
      }
    }

    // Affine transform
    if (baton->affineMatrix.size() > 0) {
      MultiPageUnsupported(nPages, "Affine");
      std::vector<double> background;
      std::tie(image, background) = sharp::ApplyAlpha(image, baton->affineBackground, shouldPremultiplyAlpha);
      vips::VInterpolate interp = vips::VInterpolate::new_from_name(
        const_cast<char*>(baton->affineInterpolator.data()));
      image = image.affine(baton->affineMatrix, VImage::option()->set("background", background)
        ->set("idx", baton->affineIdx)
        ->set("idy", baton->affineIdy)
        ->set("odx", baton->affineOdx)
        ->set("ody", baton->affineOdy)
        ->set("interpolate", interp));
    }

    // Extend edges
    if (baton->extendTop > 0 || baton->extendBottom > 0 || baton->extendLeft > 0 || baton->extendRight > 0) {
      std::vector<double> background;
      std::tie(image, background) = sharp::ApplyAlpha(image, baton->extendBackground, shouldPremultiplyAlpha);

      // Embed
      baton->width = image.width() + baton->extendLeft + baton->extendRight;
      baton->height = (nPages > 1 ? targetPageHeight : image.height()) + baton->extendTop + baton->extendBottom;

      image = nPages > 1
        ? sharp::EmbedMultiPage(image,
            baton->extendLeft, baton->extendTop, baton->width, baton->height, background, nPages, &targetPageHeight)
        : image.embed(baton->extendLeft, baton->extendTop, baton->width, baton->height,
            VImage::option()->set("extend", VIPS_EXTEND_BACKGROUND)->set("background", background));
    }
    // Median - must happen before blurring, due to the utility of blurring after thresholding
    if (shouldApplyMedian) {
      image = image.median(baton->medianSize);
    }
    // Threshold - must happen before blurring, due to the utility of blurring after thresholding
    if (baton->threshold != 0) {
      image = sharp::Threshold(image, baton->threshold, baton->thresholdGrayscale);
    }

    // Blur
    if (shouldBlur) {
      image = sharp::Blur(image, baton->blurSigma);
    }

    // Convolve
    if (shouldConv) {
      image = sharp::Convolve(image,
        baton->convKernelWidth, baton->convKernelHeight,
        baton->convKernelScale, baton->convKernelOffset,
        baton->convKernel);
    }

    // Recomb
    if (baton->recombMatrix != NULL) {
      image = sharp::Recomb(image, baton->recombMatrix);
    }

    if (shouldModulate) {
      image = sharp::Modulate(image, baton->brightness, baton->saturation, baton->hue, baton->lightness);
    }

    // Sharpen
    if (shouldSharpen) {
      image = sharp::Sharpen(image, baton->sharpenSigma, baton->sharpenM1, baton->sharpenM2,
        baton->sharpenX1, baton->sharpenY2, baton->sharpenY3);
    }

    // Composite
    if (shouldComposite) {
      std::vector<VImage> images = { image };
      std::vector<int> modes, xs, ys;
      for (Composite *composite : baton->composite) {
        VImage compositeImage;
        sharp::ImageType compositeImageType = sharp::ImageType::UNKNOWN;
        std::tie(compositeImage, compositeImageType) = sharp::OpenInput(composite->input);
        compositeImage = sharp::EnsureColourspace(compositeImage, baton->colourspaceInput);
        // Verify within current dimensions
        if (compositeImage.width() > image.width() || compositeImage.height() > image.height()) {
          throw vips::VError("Image to composite must have same dimensions or smaller");
        }
        // Check if overlay is tiled
        if (composite->tile) {
          int across = 0;
          int down = 0;
          // Use gravity in overlay
          if (compositeImage.width() <= baton->width) {
            across = static_cast<int>(ceil(static_cast<double>(image.width()) / compositeImage.width()));
            // Ensure odd number of tiles across when gravity is centre, north or south
            if (composite->gravity == 0 || composite->gravity == 1 || composite->gravity == 3) {
              across |= 1;
            }
          }
          if (compositeImage.height() <= baton->height) {
            down = static_cast<int>(ceil(static_cast<double>(image.height()) / compositeImage.height()));
            // Ensure odd number of tiles down when gravity is centre, east or west
            if (composite->gravity == 0 || composite->gravity == 2 || composite->gravity == 4) {
              down |= 1;
            }
          }
          if (across != 0 || down != 0) {
            int left;
            int top;
            compositeImage = compositeImage.replicate(across, down);
            if (composite->hasOffset) {
              std::tie(left, top) = sharp::CalculateCrop(
                compositeImage.width(), compositeImage.height(), image.width(), image.height(),
                composite->left, composite->top);
            } else {
              std::tie(left, top) = sharp::CalculateCrop(
                compositeImage.width(), compositeImage.height(), image.width(), image.height(), composite->gravity);
            }
            compositeImage = compositeImage.extract_area(left, top, image.width(), image.height());
          }
          // gravity was used for extract_area, set it back to its default value of 0
          composite->gravity = 0;
        }
        // Ensure image to composite is sRGB with unpremultiplied alpha
        compositeImage = compositeImage.colourspace(VIPS_INTERPRETATION_sRGB);
        if (!sharp::HasAlpha(compositeImage)) {
          compositeImage = sharp::EnsureAlpha(compositeImage, 1);
        }
        if (composite->premultiplied) compositeImage = compositeImage.unpremultiply();
        // Calculate position
        int left;
        int top;
        if (composite->hasOffset) {
          // Composite image at given offsets
          if (composite->tile) {
            std::tie(left, top) = sharp::CalculateCrop(image.width(), image.height(),
              compositeImage.width(), compositeImage.height(), composite->left, composite->top);
          } else {
            left = composite->left;
            top = composite->top;
          }
        } else {
          // Composite image with given gravity
          std::tie(left, top) = sharp::CalculateCrop(image.width(), image.height(),
            compositeImage.width(), compositeImage.height(), composite->gravity);
        }
        images.push_back(compositeImage);
        modes.push_back(composite->mode);
        xs.push_back(left);
        ys.push_back(top);
      }
      image = image.composite(images, modes, VImage::option()->set("x", xs)->set("y", ys));
    }

    // Reverse premultiplication after all transformations:
    if (shouldPremultiplyAlpha) {
      image = image.unpremultiply();
      // Cast pixel values to integer
      if (sharp::Is16Bit(image.interpretation())) {
        image = image.cast(VIPS_FORMAT_USHORT);
      } else {
        image = image.cast(VIPS_FORMAT_UCHAR);
      }
    }
    baton->premultiplied = shouldPremultiplyAlpha;

    // Gamma decoding (brighten)
    if (baton->gammaOut >= 1 && baton->gammaOut <= 3) {
      image = sharp::Gamma(image, baton->gammaOut);
    }

    // Linear adjustment (a * in + b)
    if (baton->linearA != 1.0 || baton->linearB != 0.0) {
      image = sharp::Linear(image, baton->linearA, baton->linearB);
    }

    // Apply normalisation - stretch luminance to cover full dynamic range
    if (baton->normalise) {
      image = sharp::Normalise(image);
    }

    // Apply contrast limiting adaptive histogram equalization (CLAHE)
    if (shouldApplyClahe) {
      image = sharp::Clahe(image, baton->claheWidth, baton->claheHeight, baton->claheMaxSlope);
    }

    // Apply bitwise boolean operation between images
    if (baton->boolean != nullptr) {
      VImage booleanImage;
      sharp::ImageType booleanImageType = sharp::ImageType::UNKNOWN;
      std::tie(booleanImage, booleanImageType) = sharp::OpenInput(baton->boolean);
      booleanImage = sharp::EnsureColourspace(booleanImage, baton->colourspaceInput);
      image = sharp::Boolean(image, booleanImage, baton->booleanOp);
    }

    // Apply per-channel Bandbool bitwise operations after all other operations
    if (baton->bandBoolOp >= VIPS_OPERATION_BOOLEAN_AND && baton->bandBoolOp < VIPS_OPERATION_BOOLEAN_LAST) {
      image = sharp::Bandbool(image, baton->bandBoolOp);
    }

    // Tint the image
    if (baton->tintA < 128.0 || baton->tintB < 128.0) {
      image = sharp::Tint(image, baton->tintA, baton->tintB);
    }

    // Extract an image channel (aka vips band)
    if (baton->extractChannel > -1) {
      if (baton->extractChannel >= image.bands()) {
        if (baton->extractChannel == 3 && sharp::HasAlpha(image)) {
          baton->extractChannel = image.bands() - 1;
        } else {
          (baton->err).append("Cannot extract channel from image. Too few channels in image.");
          return Error();
        }
      }
      VipsInterpretation const interpretation = sharp::Is16Bit(image.interpretation())
        ? VIPS_INTERPRETATION_GREY16
        : VIPS_INTERPRETATION_B_W;
      image = image
        .extract_band(baton->extractChannel)
        .copy(VImage::option()->set("interpretation", interpretation));
    }

    // Remove alpha channel, if any
    if (baton->removeAlpha) {
      image = sharp::RemoveAlpha(image);
    }

    // Ensure alpha channel, if missing
    if (baton->ensureAlpha != -1) {
      image = sharp::EnsureAlpha(image, baton->ensureAlpha);
    }

    // Convert image to sRGB, if not already
    if (sharp::Is16Bit(image.interpretation())) {
      image = image.cast(VIPS_FORMAT_USHORT);
    }
    if (image.interpretation() != baton->colourspace) {
      // Convert colourspace, pass the current known interpretation so libvips doesn't have to guess
      image = image.colourspace(baton->colourspace, VImage::option()->set("source_space", image.interpretation()));
      // Transform colours from embedded profile to output profile
      if (baton->withMetadata && sharp::HasProfile(image) && baton->withMetadataIcc.empty()) {
        image = image.icc_transform("srgb", VImage::option()
          ->set("embedded", TRUE)
          ->set("intent", VIPS_INTENT_PERCEPTUAL));
      }
    }

    // Apply output ICC profile
    if (!baton->withMetadataIcc.empty()) {
      image = image.icc_transform(
        const_cast<char*>(baton->withMetadataIcc.data()),
        VImage::option()
          ->set("input_profile", processingProfile)
          ->set("embedded", TRUE)
          ->set("intent", VIPS_INTENT_PERCEPTUAL));
    }
    // Override EXIF Orientation tag
    if (baton->withMetadata && baton->withMetadataOrientation != -1) {
      image = sharp::SetExifOrientation(image, baton->withMetadataOrientation);
    }
    // Override pixel density
    if (baton->withMetadataDensity > 0) {
      image = sharp::SetDensity(image, baton->withMetadataDensity);
    }
    // Metadata key/value pairs, e.g. EXIF
    if (!baton->withMetadataStrs.empty()) {
      image = image.copy();
      for (const auto& s : baton->withMetadataStrs) {
        image.set(s.first.data(), s.second.data());
      }
    }

    // Number of channels used in output image
    baton->channels = image.bands();
    baton->width = image.width();
    baton->height = image.height();

    image = sharp::SetAnimationProperties(
      image, nPages, targetPageHeight, baton->delay, baton->loop);

    // Output
    sharp::SetTimeout(image, baton->timeoutSeconds);
    if (baton->fileOut.empty()) {
      // Buffer output
      if (baton->formatOut == "jpeg" || (baton->formatOut == "input" && inputImageType == sharp::ImageType::JPEG)) {
        // Write JPEG to buffer
        sharp::AssertImageTypeDimensions(image, sharp::ImageType::JPEG);
        VipsArea *area = reinterpret_cast<VipsArea*>(image.jpegsave_buffer(VImage::option()
          ->set("strip", !baton->withMetadata)
          ->set("Q", baton->jpegQuality)
          ->set("interlace", baton->jpegProgressive)
          ->set("subsample_mode", baton->jpegChromaSubsampling == "4:4:4"
            ? VIPS_FOREIGN_SUBSAMPLE_OFF
            : VIPS_FOREIGN_SUBSAMPLE_ON)
          ->set("trellis_quant", baton->jpegTrellisQuantisation)
          ->set("quant_table", baton->jpegQuantisationTable)
          ->set("overshoot_deringing", baton->jpegOvershootDeringing)
          ->set("optimize_scans", baton->jpegOptimiseScans)
          ->set("optimize_coding", baton->jpegOptimiseCoding)));
        baton->bufferOut = static_cast<char*>(area->data);
        baton->bufferOutLength = area->length;
        area->free_fn = nullptr;
        vips_area_unref(area);
        baton->formatOut = "jpeg";
        if (baton->colourspace == VIPS_INTERPRETATION_CMYK) {
          baton->channels = std::min(baton->channels, 4);
        } else {
          baton->channels = std::min(baton->channels, 3);
        }
      } else if (baton->formatOut == "jp2" || (baton->formatOut == "input"
        && inputImageType == sharp::ImageType::JP2)) {
        // Write JP2 to Buffer
        sharp::AssertImageTypeDimensions(image, sharp::ImageType::JP2);
        VipsArea *area = reinterpret_cast<VipsArea*>(image.jp2ksave_buffer(VImage::option()
          ->set("Q", baton->jp2Quality)
          ->set("lossless", baton->jp2Lossless)
          ->set("subsample_mode", baton->jp2ChromaSubsampling == "4:4:4"
            ? VIPS_FOREIGN_SUBSAMPLE_OFF : VIPS_FOREIGN_SUBSAMPLE_ON)
          ->set("tile_height", baton->jp2TileHeight)
          ->set("tile_width", baton->jp2TileWidth)));
        baton->bufferOut = static_cast<char*>(area->data);
        baton->bufferOutLength = area->length;
        area->free_fn = nullptr;
        vips_area_unref(area);
        baton->formatOut = "jp2";
      } else if (baton->formatOut == "png" || (baton->formatOut == "input" &&
        (inputImageType == sharp::ImageType::PNG || inputImageType == sharp::ImageType::SVG))) {
        // Write PNG to buffer
        sharp::AssertImageTypeDimensions(image, sharp::ImageType::PNG);
        VipsArea *area = reinterpret_cast<VipsArea*>(image.pngsave_buffer(VImage::option()
          ->set("strip", !baton->withMetadata)
          ->set("interlace", baton->pngProgressive)
          ->set("compression", baton->pngCompressionLevel)
          ->set("filter", baton->pngAdaptiveFiltering ? VIPS_FOREIGN_PNG_FILTER_ALL : VIPS_FOREIGN_PNG_FILTER_NONE)
          ->set("palette", baton->pngPalette)
          ->set("Q", baton->pngQuality)
          ->set("effort", baton->pngEffort)
          ->set("bitdepth", sharp::Is16Bit(image.interpretation()) ? 16 : baton->pngBitdepth)
          ->set("dither", baton->pngDither)));
        baton->bufferOut = static_cast<char*>(area->data);
        baton->bufferOutLength = area->length;
        area->free_fn = nullptr;
        vips_area_unref(area);
        baton->formatOut = "png";
      } else if (baton->formatOut == "webp" ||
        (baton->formatOut == "input" && inputImageType == sharp::ImageType::WEBP)) {
        // Write WEBP to buffer
        sharp::AssertImageTypeDimensions(image, sharp::ImageType::WEBP);
        VipsArea *area = reinterpret_cast<VipsArea*>(image.webpsave_buffer(VImage::option()
          ->set("strip", !baton->withMetadata)
          ->set("Q", baton->webpQuality)
          ->set("lossless", baton->webpLossless)
          ->set("near_lossless", baton->webpNearLossless)
          ->set("smart_subsample", baton->webpSmartSubsample)
          ->set("effort", baton->webpEffort)
          ->set("alpha_q", baton->webpAlphaQuality)));
        baton->bufferOut = static_cast<char*>(area->data);
        baton->bufferOutLength = area->length;
        area->free_fn = nullptr;
        vips_area_unref(area);
        baton->formatOut = "webp";
      } else if (baton->formatOut == "gif" ||
        (baton->formatOut == "input" && inputImageType == sharp::ImageType::GIF)) {
        // Write GIF to buffer
        sharp::AssertImageTypeDimensions(image, sharp::ImageType::GIF);
        VipsArea *area = reinterpret_cast<VipsArea*>(image.gifsave_buffer(VImage::option()
          ->set("strip", !baton->withMetadata)
          ->set("bitdepth", baton->gifBitdepth)
          ->set("effort", baton->gifEffort)
          ->set("dither", baton->gifDither)));
        baton->bufferOut = static_cast<char*>(area->data);
        baton->bufferOutLength = area->length;
        area->free_fn = nullptr;
        vips_area_unref(area);
        baton->formatOut = "gif";
      } else if (baton->formatOut == "tiff" ||
        (baton->formatOut == "input" && inputImageType == sharp::ImageType::TIFF)) {
        // Write TIFF to buffer
        if (baton->tiffCompression == VIPS_FOREIGN_TIFF_COMPRESSION_JPEG) {
          sharp::AssertImageTypeDimensions(image, sharp::ImageType::JPEG);
          baton->channels = std::min(baton->channels, 3);
        }
        // Cast pixel values to float, if required
        if (baton->tiffPredictor == VIPS_FOREIGN_TIFF_PREDICTOR_FLOAT) {
          image = image.cast(VIPS_FORMAT_FLOAT);
        }
        VipsArea *area = reinterpret_cast<VipsArea*>(image.tiffsave_buffer(VImage::option()
          ->set("strip", !baton->withMetadata)
          ->set("Q", baton->tiffQuality)
          ->set("bitdepth", baton->tiffBitdepth)
          ->set("compression", baton->tiffCompression)
          ->set("predictor", baton->tiffPredictor)
          ->set("pyramid", baton->tiffPyramid)
          ->set("tile", baton->tiffTile)
          ->set("tile_height", baton->tiffTileHeight)
          ->set("tile_width", baton->tiffTileWidth)
          ->set("xres", baton->tiffXres)
          ->set("yres", baton->tiffYres)
          ->set("resunit", baton->tiffResolutionUnit)));
        baton->bufferOut = static_cast<char*>(area->data);
        baton->bufferOutLength = area->length;
        area->free_fn = nullptr;
        vips_area_unref(area);
        baton->formatOut = "tiff";
      } else if (baton->formatOut == "heif" ||
        (baton->formatOut == "input" && inputImageType == sharp::ImageType::HEIF)) {
        // Write HEIF to buffer
        image = sharp::RemoveAnimationProperties(image);
        VipsArea *area = reinterpret_cast<VipsArea*>(image.heifsave_buffer(VImage::option()
          ->set("strip", !baton->withMetadata)
          ->set("Q", baton->heifQuality)
          ->set("compression", baton->heifCompression)
          ->set("effort", baton->heifEffort)
          ->set("subsample_mode", baton->heifChromaSubsampling == "4:4:4"
            ? VIPS_FOREIGN_SUBSAMPLE_OFF : VIPS_FOREIGN_SUBSAMPLE_ON)
          ->set("lossless", baton->heifLossless)));
        baton->bufferOut = static_cast<char*>(area->data);
        baton->bufferOutLength = area->length;
        area->free_fn = nullptr;
        vips_area_unref(area);
        baton->formatOut = "heif";
      } else if (baton->formatOut == "raw" ||
        (baton->formatOut == "input" && inputImageType == sharp::ImageType::RAW)) {
        // Write raw, uncompressed image data to buffer
        if (baton->greyscale || image.interpretation() == VIPS_INTERPRETATION_B_W) {
          // Extract first band for greyscale image
          image = image[0];
          baton->channels = 1;
        }
        if (image.format() != baton->rawDepth) {
          // Cast pixels to requested format
          image = image.cast(baton->rawDepth);
        }
        // Get raw image data
        baton->bufferOut = static_cast<char*>(image.write_to_memory(&baton->bufferOutLength));
        if (baton->bufferOut == nullptr) {
          (baton->err).append("Could not allocate enough memory for raw output");
          return Error();
        }
        baton->formatOut = "raw";
      } else {
        // Unsupported output format
        (baton->err).append("Unsupported output format ");
        if (baton->formatOut == "input") {
          (baton->err).append(ImageTypeId(inputImageType));
        } else {
          (baton->err).append(baton->formatOut);
        }
        return Error();
      }
    } else {
      // File output
      bool const isJpeg = sharp::IsJpeg(baton->fileOut);
      bool const isPng = sharp::IsPng(baton->fileOut);
      bool const isWebp = sharp::IsWebp(baton->fileOut);
      bool const isGif = sharp::IsGif(baton->fileOut);
      bool const isTiff = sharp::IsTiff(baton->fileOut);
      bool const isJp2 = sharp::IsJp2(baton->fileOut);
      bool const isHeif = sharp::IsHeif(baton->fileOut);
      bool const isDz = sharp::IsDz(baton->fileOut);
      bool const isDzZip = sharp::IsDzZip(baton->fileOut);
      bool const isV = sharp::IsV(baton->fileOut);
      bool const mightMatchInput = baton->formatOut == "input";
      bool const willMatchInput = mightMatchInput &&
       !(isJpeg || isPng || isWebp || isGif || isTiff || isJp2 || isHeif || isDz || isDzZip || isV);

      if (baton->formatOut == "jpeg" || (mightMatchInput && isJpeg) ||
        (willMatchInput && inputImageType == sharp::ImageType::JPEG)) {
        // Write JPEG to file
        sharp::AssertImageTypeDimensions(image, sharp::ImageType::JPEG);
        image.jpegsave(const_cast<char*>(baton->fileOut.data()), VImage::option()
          ->set("strip", !baton->withMetadata)
          ->set("Q", baton->jpegQuality)
          ->set("interlace", baton->jpegProgressive)
          ->set("subsample_mode", baton->jpegChromaSubsampling == "4:4:4"
            ? VIPS_FOREIGN_SUBSAMPLE_OFF
            : VIPS_FOREIGN_SUBSAMPLE_ON)
          ->set("trellis_quant", baton->jpegTrellisQuantisation)
          ->set("quant_table", baton->jpegQuantisationTable)
          ->set("overshoot_deringing", baton->jpegOvershootDeringing)
          ->set("optimize_scans", baton->jpegOptimiseScans)
          ->set("optimize_coding", baton->jpegOptimiseCoding));
        baton->formatOut = "jpeg";
        baton->channels = std::min(baton->channels, 3);
      } else if (baton->formatOut == "jp2" || (mightMatchInput && isJp2) ||
        (willMatchInput && (inputImageType == sharp::ImageType::JP2))) {
        // Write JP2 to file
        sharp::AssertImageTypeDimensions(image, sharp::ImageType::JP2);
        image.jp2ksave(const_cast<char*>(baton->fileOut.data()), VImage::option()
          ->set("Q", baton->jp2Quality)
          ->set("lossless", baton->jp2Lossless)
          ->set("subsample_mode", baton->jp2ChromaSubsampling == "4:4:4"
            ? VIPS_FOREIGN_SUBSAMPLE_OFF : VIPS_FOREIGN_SUBSAMPLE_ON)
          ->set("tile_height", baton->jp2TileHeight)
          ->set("tile_width", baton->jp2TileWidth));
          baton->formatOut = "jp2";
      } else if (baton->formatOut == "png" || (mightMatchInput && isPng) || (willMatchInput &&
        (inputImageType == sharp::ImageType::PNG || inputImageType == sharp::ImageType::SVG))) {
        // Write PNG to file
        sharp::AssertImageTypeDimensions(image, sharp::ImageType::PNG);
        image.pngsave(const_cast<char*>(baton->fileOut.data()), VImage::option()
          ->set("strip", !baton->withMetadata)
          ->set("interlace", baton->pngProgressive)
          ->set("compression", baton->pngCompressionLevel)
          ->set("filter", baton->pngAdaptiveFiltering ? VIPS_FOREIGN_PNG_FILTER_ALL : VIPS_FOREIGN_PNG_FILTER_NONE)
          ->set("palette", baton->pngPalette)
          ->set("Q", baton->pngQuality)
          ->set("bitdepth", sharp::Is16Bit(image.interpretation()) ? 16 : baton->pngBitdepth)
          ->set("effort", baton->pngEffort)
          ->set("dither", baton->pngDither));
        baton->formatOut = "png";
      } else if (baton->formatOut == "webp" || (mightMatchInput && isWebp) ||
        (willMatchInput && inputImageType == sharp::ImageType::WEBP)) {
        // Write WEBP to file
        sharp::AssertImageTypeDimensions(image, sharp::ImageType::WEBP);
        image.webpsave(const_cast<char*>(baton->fileOut.data()), VImage::option()
          ->set("strip", !baton->withMetadata)
          ->set("Q", baton->webpQuality)
          ->set("lossless", baton->webpLossless)
          ->set("near_lossless", baton->webpNearLossless)
          ->set("smart_subsample", baton->webpSmartSubsample)
          ->set("effort", baton->webpEffort)
          ->set("alpha_q", baton->webpAlphaQuality));
        baton->formatOut = "webp";
      } else if (baton->formatOut == "gif" || (mightMatchInput && isGif) ||
        (willMatchInput && inputImageType == sharp::ImageType::GIF)) {
        // Write GIF to file
        sharp::AssertImageTypeDimensions(image, sharp::ImageType::GIF);
        image.gifsave(const_cast<char*>(baton->fileOut.data()), VImage::option()
          ->set("strip", !baton->withMetadata)
          ->set("bitdepth", baton->gifBitdepth)
          ->set("effort", baton->gifEffort)
          ->set("dither", baton->gifDither));
        baton->formatOut = "gif";
      } else if (baton->formatOut == "tiff" || (mightMatchInput && isTiff) ||
        (willMatchInput && inputImageType == sharp::ImageType::TIFF)) {
        // Write TIFF to file
        if (baton->tiffCompression == VIPS_FOREIGN_TIFF_COMPRESSION_JPEG) {
          sharp::AssertImageTypeDimensions(image, sharp::ImageType::JPEG);
          baton->channels = std::min(baton->channels, 3);
        }
        // Cast pixel values to float, if required
        if (baton->tiffPredictor == VIPS_FOREIGN_TIFF_PREDICTOR_FLOAT) {
          image = image.cast(VIPS_FORMAT_FLOAT);
        }
        image.tiffsave(const_cast<char*>(baton->fileOut.data()), VImage::option()
          ->set("strip", !baton->withMetadata)
          ->set("Q", baton->tiffQuality)
          ->set("bitdepth", baton->tiffBitdepth)
          ->set("compression", baton->tiffCompression)
          ->set("predictor", baton->tiffPredictor)
          ->set("pyramid", baton->tiffPyramid)
          ->set("tile", baton->tiffTile)
          ->set("tile_height", baton->tiffTileHeight)
          ->set("tile_width", baton->tiffTileWidth)
          ->set("xres", baton->tiffXres)
          ->set("yres", baton->tiffYres)
          ->set("resunit", baton->tiffResolutionUnit));
        baton->formatOut = "tiff";
      } else if (baton->formatOut == "heif" || (mightMatchInput && isHeif) ||
        (willMatchInput && inputImageType == sharp::ImageType::HEIF)) {
        // Write HEIF to file
        image = sharp::RemoveAnimationProperties(image);
        image.heifsave(const_cast<char*>(baton->fileOut.data()), VImage::option()
          ->set("strip", !baton->withMetadata)
          ->set("Q", baton->heifQuality)
          ->set("compression", baton->heifCompression)
          ->set("effort", baton->heifEffort)
          ->set("subsample_mode", baton->heifChromaSubsampling == "4:4:4"
            ? VIPS_FOREIGN_SUBSAMPLE_OFF : VIPS_FOREIGN_SUBSAMPLE_ON)
          ->set("lossless", baton->heifLossless));
        baton->formatOut = "heif";
      } else if (baton->formatOut == "dz" || isDz || isDzZip) {
        if (isDzZip) {
          baton->tileContainer = VIPS_FOREIGN_DZ_CONTAINER_ZIP;
        }
        // Forward format options through suffix
        std::string suffix;
        if (baton->tileFormat == "png") {
          std::vector<std::pair<std::string, std::string>> options {
            {"interlace", baton->pngProgressive ? "TRUE" : "FALSE"},
            {"compression", std::to_string(baton->pngCompressionLevel)},
            {"filter", baton->pngAdaptiveFiltering ? "all" : "none"}
          };
          suffix = AssembleSuffixString(".png", options);
        } else if (baton->tileFormat == "webp") {
          std::vector<std::pair<std::string, std::string>> options {
            {"Q", std::to_string(baton->webpQuality)},
            {"alpha_q", std::to_string(baton->webpAlphaQuality)},
            {"lossless", baton->webpLossless ? "TRUE" : "FALSE"},
            {"near_lossless", baton->webpNearLossless ? "TRUE" : "FALSE"},
            {"smart_subsample", baton->webpSmartSubsample ? "TRUE" : "FALSE"},
            {"effort", std::to_string(baton->webpEffort)}
          };
          suffix = AssembleSuffixString(".webp", options);
        } else {
          std::vector<std::pair<std::string, std::string>> options {
            {"Q", std::to_string(baton->jpegQuality)},
            {"interlace", baton->jpegProgressive ? "TRUE" : "FALSE"},
            {"subsample_mode", baton->jpegChromaSubsampling == "4:4:4" ? "off" : "on"},
            {"trellis_quant", baton->jpegTrellisQuantisation ? "TRUE" : "FALSE"},
            {"quant_table", std::to_string(baton->jpegQuantisationTable)},
            {"overshoot_deringing", baton->jpegOvershootDeringing ? "TRUE": "FALSE"},
            {"optimize_scans", baton->jpegOptimiseScans ? "TRUE": "FALSE"},
            {"optimize_coding", baton->jpegOptimiseCoding ? "TRUE": "FALSE"}
          };
          std::string extname = baton->tileLayout == VIPS_FOREIGN_DZ_LAYOUT_DZ ? ".jpeg" : ".jpg";
          suffix = AssembleSuffixString(extname, options);
        }
        // Remove alpha channel from tile background if image does not contain an alpha channel
        if (!sharp::HasAlpha(image)) {
          baton->tileBackground.pop_back();
        }
        // Write DZ to file
        vips::VOption *options = VImage::option()
          ->set("strip", !baton->withMetadata)
          ->set("tile_size", baton->tileSize)
          ->set("overlap", baton->tileOverlap)
          ->set("container", baton->tileContainer)
          ->set("layout", baton->tileLayout)
          ->set("suffix", const_cast<char*>(suffix.data()))
          ->set("angle", CalculateAngleRotation(baton->tileAngle))
          ->set("background", baton->tileBackground)
          ->set("centre", baton->tileCentre)
          ->set("id", const_cast<char*>(baton->tileId.data()))
          ->set("skip_blanks", baton->tileSkipBlanks);
        // libvips chooses a default depth based on layout. Instead of replicating that logic here by
        // not passing anything - libvips will handle choice
        if (baton->tileDepth < VIPS_FOREIGN_DZ_DEPTH_LAST) {
          options->set("depth", baton->tileDepth);
        }
        image.dzsave(const_cast<char*>(baton->fileOut.data()), options);
        baton->formatOut = "dz";
      } else if (baton->formatOut == "v" || (mightMatchInput && isV) ||
        (willMatchInput && inputImageType == sharp::ImageType::VIPS)) {
        // Write V to file
        image.vipssave(const_cast<char*>(baton->fileOut.data()), VImage::option()
          ->set("strip", !baton->withMetadata));
        baton->formatOut = "v";
      } else {
        // Unsupported output format
        (baton->err).append("Unsupported output format " + baton->fileOut);
        return Error();
      }
    }
  } catch (vips::VError const &err) {
    char const *what = err.what();
    if (what && what[0]) {
      (baton->err).append(what);
    } else {
      (baton->err).append("Unknown error");
    }
  }
  // Clean up libvips' per-request data and threads
  vips_error_clear();
  vips_thread_shutdown();
}

PipelineBaton* CreatePipelineBaton() {
  return new PipelineBaton;
}

void DestroyPipelineBaton(PipelineBaton* baton) {
  delete baton->input;
  delete baton->boolean;
  for (Composite *composite : baton->composite) {
    delete composite->input;
    delete composite;
  }
  for (InputDescriptor *input : baton->joinChannelIn) {
    delete input;
  }
  delete baton;
}

InputDescriptor* PipelineBaton_GetInput(PipelineBaton* baton) { return baton->input; }
void PipelineBaton_SetInput(PipelineBaton* baton, InputDescriptor* val) { baton->input = val; }
const char* PipelineBaton_GetFormatOut(PipelineBaton* baton) { return baton->formatOut.c_str(); }
void PipelineBaton_SetFormatOut(PipelineBaton* baton, const char* val) { baton->formatOut = val; }
const char* PipelineBaton_GetFileOut(PipelineBaton* baton) { return baton->fileOut.c_str(); }
void PipelineBaton_SetFileOut(PipelineBaton* baton, const char* val) { baton->fileOut = val; }
void* PipelineBaton_GetBufferOut(PipelineBaton* baton) { return baton->bufferOut; }
void PipelineBaton_SetBufferOut(PipelineBaton* baton, void* val) { baton->bufferOut = val; }
size_t PipelineBaton_GetBufferOutLength(PipelineBaton* baton) { return baton->bufferOutLength; }
void PipelineBaton_SetBufferOutLength(PipelineBaton* baton, size_t val) { baton->bufferOutLength = val; }
Composite ** PipelineBaton_GetComposite(PipelineBaton* baton) { return baton->composite.data(); }
void PipelineBaton_SetComposite(PipelineBaton* baton, std::vector<Composite *> val) { baton->composite = val; }
InputDescriptor ** PipelineBaton_GetJoinChannelIn(PipelineBaton* baton) { return baton->joinChannelIn.data(); }
void PipelineBaton_SetJoinChannelIn(PipelineBaton* baton, std::vector<InputDescriptor *> val) { baton->joinChannelIn = val; }
int PipelineBaton_GetTopOffsetPre(PipelineBaton* baton) { return baton->topOffsetPre; }
void PipelineBaton_SetTopOffsetPre(PipelineBaton* baton, int val) { baton->topOffsetPre = val; }
int PipelineBaton_GetLeftOffsetPre(PipelineBaton* baton) { return baton->leftOffsetPre; }
void PipelineBaton_SetLeftOffsetPre(PipelineBaton* baton, int val) { baton->leftOffsetPre = val; }
int PipelineBaton_GetWidthPre(PipelineBaton* baton) { return baton->widthPre; }
void PipelineBaton_SetWidthPre(PipelineBaton* baton, int val) { baton->widthPre = val; }
int PipelineBaton_GetHeightPre(PipelineBaton* baton) { return baton->heightPre; }
void PipelineBaton_SetHeightPre(PipelineBaton* baton, int val) { baton->heightPre = val; }
int PipelineBaton_GetTopOffsetPost(PipelineBaton* baton) { return baton->topOffsetPost; }
void PipelineBaton_SetTopOffsetPost(PipelineBaton* baton, int val) { baton->topOffsetPost = val; }
int PipelineBaton_GetLeftOffsetPost(PipelineBaton* baton) { return baton->leftOffsetPost; }
void PipelineBaton_SetLeftOffsetPost(PipelineBaton* baton, int val) { baton->leftOffsetPost = val; }
int PipelineBaton_GetWidthPost(PipelineBaton* baton) { return baton->widthPost; }
void PipelineBaton_SetWidthPost(PipelineBaton* baton, int val) { baton->widthPost = val; }
int PipelineBaton_GetHeightPost(PipelineBaton* baton) { return baton->heightPost; }
void PipelineBaton_SetHeightPost(PipelineBaton* baton, int val) { baton->heightPost = val; }
int PipelineBaton_GetWidth(PipelineBaton* baton) { return baton->width; }
void PipelineBaton_SetWidth(PipelineBaton* baton, int val) { baton->width = val; }
int PipelineBaton_GetHeight(PipelineBaton* baton) { return baton->height; }
void PipelineBaton_SetHeight(PipelineBaton* baton, int val) { baton->height = val; }
int PipelineBaton_GetChannels(PipelineBaton* baton) { return baton->channels; }
void PipelineBaton_SetChannels(PipelineBaton* baton, int val) { baton->channels = val; }
sharp::Canvas PipelineBaton_GetCanvas(PipelineBaton* baton) { return baton->canvas; }
void PipelineBaton_SetCanvas(PipelineBaton* baton, sharp::Canvas val) { baton->canvas = val; }
int PipelineBaton_GetPosition(PipelineBaton* baton) { return baton->position; }
void PipelineBaton_SetPosition(PipelineBaton* baton, int val) { baton->position = val; }
double* PipelineBaton_GetResizeBackground(PipelineBaton* baton) { return baton->resizeBackground.data(); }
void PipelineBaton_SetResizeBackground(PipelineBaton* baton, std::vector<double> val) { baton->resizeBackground = val; }
bool PipelineBaton_GetHasCropOffset(PipelineBaton* baton) { return baton->hasCropOffset; }
void PipelineBaton_SetHasCropOffset(PipelineBaton* baton, bool val) { baton->hasCropOffset = val; }
int PipelineBaton_GetCropOffsetLeft(PipelineBaton* baton) { return baton->cropOffsetLeft; }
void PipelineBaton_SetCropOffsetLeft(PipelineBaton* baton, int val) { baton->cropOffsetLeft = val; }
int PipelineBaton_GetCropOffsetTop(PipelineBaton* baton) { return baton->cropOffsetTop; }
void PipelineBaton_SetCropOffsetTop(PipelineBaton* baton, int val) { baton->cropOffsetTop = val; }
bool PipelineBaton_GetPremultiplied(PipelineBaton* baton) { return baton->premultiplied; }
void PipelineBaton_SetPremultiplied(PipelineBaton* baton, bool val) { baton->premultiplied = val; }
bool PipelineBaton_GetTileCentre(PipelineBaton* baton) { return baton->tileCentre; }
void PipelineBaton_SetTileCentre(PipelineBaton* baton, bool val) { baton->tileCentre = val; }
const char* PipelineBaton_GetKernel(PipelineBaton* baton) { return baton->kernel.c_str(); }
void PipelineBaton_SetKernel(PipelineBaton* baton, const char* val) { baton->kernel = val; }
bool PipelineBaton_GetFastShrinkOnLoad(PipelineBaton* baton) { return baton->fastShrinkOnLoad; }
void PipelineBaton_SetFastShrinkOnLoad(PipelineBaton* baton, bool val) { baton->fastShrinkOnLoad = val; }
double PipelineBaton_GetTintA(PipelineBaton* baton) { return baton->tintA; }
void PipelineBaton_SetTintA(PipelineBaton* baton, double val) { baton->tintA = val; }
double PipelineBaton_GetTintB(PipelineBaton* baton) { return baton->tintB; }
void PipelineBaton_SetTintB(PipelineBaton* baton, double val) { baton->tintB = val; }
bool PipelineBaton_GetFlatten(PipelineBaton* baton) { return baton->flatten; }
void PipelineBaton_SetFlatten(PipelineBaton* baton, bool val) { baton->flatten = val; }
double* PipelineBaton_GetFlattenBackground(PipelineBaton* baton) { return baton->flattenBackground.data(); }
void PipelineBaton_SetFlattenBackground(PipelineBaton* baton, std::vector<double> val) { baton->flattenBackground = val; }
bool PipelineBaton_GetNegate(PipelineBaton* baton) { return baton->negate; }
void PipelineBaton_SetNegate(PipelineBaton* baton, bool val) { baton->negate = val; }
bool PipelineBaton_GetNegateAlpha(PipelineBaton* baton) { return baton->negateAlpha; }
void PipelineBaton_SetNegateAlpha(PipelineBaton* baton, bool val) { baton->negateAlpha = val; }
double PipelineBaton_GetBlurSigma(PipelineBaton* baton) { return baton->blurSigma; }
void PipelineBaton_SetBlurSigma(PipelineBaton* baton, double val) { baton->blurSigma = val; }
double PipelineBaton_GetBrightness(PipelineBaton* baton) { return baton->brightness; }
void PipelineBaton_SetBrightness(PipelineBaton* baton, double val) { baton->brightness = val; }
double PipelineBaton_GetSaturation(PipelineBaton* baton) { return baton->saturation; }
void PipelineBaton_SetSaturation(PipelineBaton* baton, double val) { baton->saturation = val; }
int PipelineBaton_GetHue(PipelineBaton* baton) { return baton->hue; }
void PipelineBaton_SetHue(PipelineBaton* baton, int val) { baton->hue = val; }
double PipelineBaton_GetLightness(PipelineBaton* baton) { return baton->lightness; }
void PipelineBaton_SetLightness(PipelineBaton* baton, double val) { baton->lightness = val; }
int PipelineBaton_GetMedianSize(PipelineBaton* baton) { return baton->medianSize; }
void PipelineBaton_SetMedianSize(PipelineBaton* baton, int val) { baton->medianSize = val; }
double PipelineBaton_GetSharpenSigma(PipelineBaton* baton) { return baton->sharpenSigma; }
void PipelineBaton_SetSharpenSigma(PipelineBaton* baton, double val) { baton->sharpenSigma = val; }
double PipelineBaton_GetSharpenM1(PipelineBaton* baton) { return baton->sharpenM1; }
void PipelineBaton_SetSharpenM1(PipelineBaton* baton, double val) { baton->sharpenM1 = val; }
double PipelineBaton_GetSharpenM2(PipelineBaton* baton) { return baton->sharpenM2; }
void PipelineBaton_SetSharpenM2(PipelineBaton* baton, double val) { baton->sharpenM2 = val; }
double PipelineBaton_GetSharpenX1(PipelineBaton* baton) { return baton->sharpenX1; }
void PipelineBaton_SetSharpenX1(PipelineBaton* baton, double val) { baton->sharpenX1 = val; }
double PipelineBaton_GetSharpenY2(PipelineBaton* baton) { return baton->sharpenY2; }
void PipelineBaton_SetSharpenY2(PipelineBaton* baton, double val) { baton->sharpenY2 = val; }
double PipelineBaton_GetSharpenY3(PipelineBaton* baton) { return baton->sharpenY3; }
void PipelineBaton_SetSharpenY3(PipelineBaton* baton, double val) { baton->sharpenY3 = val; }
int PipelineBaton_GetThreshold(PipelineBaton* baton) { return baton->threshold; }
void PipelineBaton_SetThreshold(PipelineBaton* baton, int val) { baton->threshold = val; }
bool PipelineBaton_GetThresholdGrayscale(PipelineBaton* baton) { return baton->thresholdGrayscale; }
void PipelineBaton_SetThresholdGrayscale(PipelineBaton* baton, bool val) { baton->thresholdGrayscale = val; }
double PipelineBaton_GetTrimThreshold(PipelineBaton* baton) { return baton->trimThreshold; }
void PipelineBaton_SetTrimThreshold(PipelineBaton* baton, double val) { baton->trimThreshold = val; }
int PipelineBaton_GetTrimOffsetLeft(PipelineBaton* baton) { return baton->trimOffsetLeft; }
void PipelineBaton_SetTrimOffsetLeft(PipelineBaton* baton, int val) { baton->trimOffsetLeft = val; }
int PipelineBaton_GetTrimOffsetTop(PipelineBaton* baton) { return baton->trimOffsetTop; }
void PipelineBaton_SetTrimOffsetTop(PipelineBaton* baton, int val) { baton->trimOffsetTop = val; }
double PipelineBaton_GetLinearA(PipelineBaton* baton) { return baton->linearA; }
void PipelineBaton_SetLinearA(PipelineBaton* baton, double val) { baton->linearA = val; }
double PipelineBaton_GetLinearB(PipelineBaton* baton) { return baton->linearB; }
void PipelineBaton_SetLinearB(PipelineBaton* baton, double val) { baton->linearB = val; }
double PipelineBaton_GetGamma(PipelineBaton* baton) { return baton->gamma; }
void PipelineBaton_SetGamma(PipelineBaton* baton, double val) { baton->gamma = val; }
double PipelineBaton_GetGammaOut(PipelineBaton* baton) { return baton->gammaOut; }
void PipelineBaton_SetGammaOut(PipelineBaton* baton, double val) { baton->gammaOut = val; }
bool PipelineBaton_GetGreyscale(PipelineBaton* baton) { return baton->greyscale; }
void PipelineBaton_SetGreyscale(PipelineBaton* baton, bool val) { baton->greyscale = val; }
bool PipelineBaton_GetNormalise(PipelineBaton* baton) { return baton->normalise; }
void PipelineBaton_SetNormalise(PipelineBaton* baton, bool val) { baton->normalise = val; }
int PipelineBaton_GetClaheWidth(PipelineBaton* baton) { return baton->claheWidth; }
void PipelineBaton_SetClaheWidth(PipelineBaton* baton, int val) { baton->claheWidth = val; }
int PipelineBaton_GetClaheHeight(PipelineBaton* baton) { return baton->claheHeight; }
void PipelineBaton_SetClaheHeight(PipelineBaton* baton, int val) { baton->claheHeight = val; }
int PipelineBaton_GetClaheMaxSlope(PipelineBaton* baton) { return baton->claheMaxSlope; }
void PipelineBaton_SetClaheMaxSlope(PipelineBaton* baton, int val) { baton->claheMaxSlope = val; }
bool PipelineBaton_GetUseExifOrientation(PipelineBaton* baton) { return baton->useExifOrientation; }
void PipelineBaton_SetUseExifOrientation(PipelineBaton* baton, bool val) { baton->useExifOrientation = val; }
int PipelineBaton_GetAngle(PipelineBaton* baton) { return baton->angle; }
void PipelineBaton_SetAngle(PipelineBaton* baton, int val) { baton->angle = val; }
double PipelineBaton_GetRotationAngle(PipelineBaton* baton) { return baton->rotationAngle; }
void PipelineBaton_SetRotationAngle(PipelineBaton* baton, double val) { baton->rotationAngle = val; }
double* PipelineBaton_GetRotationBackground(PipelineBaton* baton) { return baton->rotationBackground.data(); }
void PipelineBaton_SetRotationBackground(PipelineBaton* baton, std::vector<double> val) { baton->rotationBackground = val; }
bool PipelineBaton_GetRotateBeforePreExtract(PipelineBaton* baton) { return baton->rotateBeforePreExtract; }
void PipelineBaton_SetRotateBeforePreExtract(PipelineBaton* baton, bool val) { baton->rotateBeforePreExtract = val; }
bool PipelineBaton_GetFlip(PipelineBaton* baton) { return baton->flip; }
void PipelineBaton_SetFlip(PipelineBaton* baton, bool val) { baton->flip = val; }
bool PipelineBaton_GetFlop(PipelineBaton* baton) { return baton->flop; }
void PipelineBaton_SetFlop(PipelineBaton* baton, bool val) { baton->flop = val; }
int PipelineBaton_GetExtendTop(PipelineBaton* baton) { return baton->extendTop; }
void PipelineBaton_SetExtendTop(PipelineBaton* baton, int val) { baton->extendTop = val; }
int PipelineBaton_GetExtendBottom(PipelineBaton* baton) { return baton->extendBottom; }
void PipelineBaton_SetExtendBottom(PipelineBaton* baton, int val) { baton->extendBottom = val; }
int PipelineBaton_GetExtendLeft(PipelineBaton* baton) { return baton->extendLeft; }
void PipelineBaton_SetExtendLeft(PipelineBaton* baton, int val) { baton->extendLeft = val; }
int PipelineBaton_GetExtendRight(PipelineBaton* baton) { return baton->extendRight; }
void PipelineBaton_SetExtendRight(PipelineBaton* baton, int val) { baton->extendRight = val; }
double* PipelineBaton_GetExtendBackground(PipelineBaton* baton) { return baton->extendBackground.data(); }
void PipelineBaton_SetExtendBackground(PipelineBaton* baton, std::vector<double> val) { baton->extendBackground = val; }
bool PipelineBaton_GetWithoutEnlargement(PipelineBaton* baton) { return baton->withoutEnlargement; }
void PipelineBaton_SetWithoutEnlargement(PipelineBaton* baton, bool val) { baton->withoutEnlargement = val; }
bool PipelineBaton_GetWithoutReduction(PipelineBaton* baton) { return baton->withoutReduction; }
void PipelineBaton_SetWithoutReduction(PipelineBaton* baton, bool val) { baton->withoutReduction = val; }
double* PipelineBaton_GetAffineMatrix(PipelineBaton* baton) { return baton->affineMatrix.data(); }
void PipelineBaton_SetAffineMatrix(PipelineBaton* baton, std::vector<double> val) { baton->affineMatrix = val; }
double* PipelineBaton_GetAffineBackground(PipelineBaton* baton) { return baton->affineBackground.data(); }
void PipelineBaton_SetAffineBackground(PipelineBaton* baton, std::vector<double> val) { baton->affineBackground = val; }
double PipelineBaton_GetAffineIdx(PipelineBaton* baton) { return baton->affineIdx; }
void PipelineBaton_SetAffineIdx(PipelineBaton* baton, double val) { baton->affineIdx = val; }
double PipelineBaton_GetAffineIdy(PipelineBaton* baton) { return baton->affineIdy; }
void PipelineBaton_SetAffineIdy(PipelineBaton* baton, double val) { baton->affineIdy = val; }
double PipelineBaton_GetAffineOdx(PipelineBaton* baton) { return baton->affineOdx; }
void PipelineBaton_SetAffineOdx(PipelineBaton* baton, double val) { baton->affineOdx = val; }
double PipelineBaton_GetAffineOdy(PipelineBaton* baton) { return baton->affineOdy; }
void PipelineBaton_SetAffineOdy(PipelineBaton* baton, double val) { baton->affineOdy = val; }
const char* PipelineBaton_GetAffineInterpolator(PipelineBaton* baton) { return baton->affineInterpolator.c_str(); }
void PipelineBaton_SetAffineInterpolator(PipelineBaton* baton, const char* val) { baton->affineInterpolator = val; }
int PipelineBaton_GetJpegQuality(PipelineBaton* baton) { return baton->jpegQuality; }
void PipelineBaton_SetJpegQuality(PipelineBaton* baton, int val) { baton->jpegQuality = val; }
bool PipelineBaton_GetJpegProgressive(PipelineBaton* baton) { return baton->jpegProgressive; }
void PipelineBaton_SetJpegProgressive(PipelineBaton* baton, bool val) { baton->jpegProgressive = val; }
const char* PipelineBaton_GetJpegChromaSubsampling(PipelineBaton* baton) { return baton->jpegChromaSubsampling.c_str(); }
void PipelineBaton_SetJpegChromaSubsampling(PipelineBaton* baton, const char* val) { baton->jpegChromaSubsampling = val; }
bool PipelineBaton_GetJpegTrellisQuantisation(PipelineBaton* baton) { return baton->jpegTrellisQuantisation; }
void PipelineBaton_SetJpegTrellisQuantisation(PipelineBaton* baton, bool val) { baton->jpegTrellisQuantisation = val; }
int PipelineBaton_GetJpegQuantisationTable(PipelineBaton* baton) { return baton->jpegQuantisationTable; }
void PipelineBaton_SetJpegQuantisationTable(PipelineBaton* baton, int val) { baton->jpegQuantisationTable = val; }
bool PipelineBaton_GetJpegOvershootDeringing(PipelineBaton* baton) { return baton->jpegOvershootDeringing; }
void PipelineBaton_SetJpegOvershootDeringing(PipelineBaton* baton, bool val) { baton->jpegOvershootDeringing = val; }
bool PipelineBaton_GetJpegOptimiseScans(PipelineBaton* baton) { return baton->jpegOptimiseScans; }
void PipelineBaton_SetJpegOptimiseScans(PipelineBaton* baton, bool val) { baton->jpegOptimiseScans = val; }
bool PipelineBaton_GetJpegOptimiseCoding(PipelineBaton* baton) { return baton->jpegOptimiseCoding; }
void PipelineBaton_SetJpegOptimiseCoding(PipelineBaton* baton, bool val) { baton->jpegOptimiseCoding = val; }
bool PipelineBaton_GetPngProgressive(PipelineBaton* baton) { return baton->pngProgressive; }
void PipelineBaton_SetPngProgressive(PipelineBaton* baton, bool val) { baton->pngProgressive = val; }
int PipelineBaton_GetPngCompressionLevel(PipelineBaton* baton) { return baton->pngCompressionLevel; }
void PipelineBaton_SetPngCompressionLevel(PipelineBaton* baton, int val) { baton->pngCompressionLevel = val; }
bool PipelineBaton_GetPngAdaptiveFiltering(PipelineBaton* baton) { return baton->pngAdaptiveFiltering; }
void PipelineBaton_SetPngAdaptiveFiltering(PipelineBaton* baton, bool val) { baton->pngAdaptiveFiltering = val; }
bool PipelineBaton_GetPngPalette(PipelineBaton* baton) { return baton->pngPalette; }
void PipelineBaton_SetPngPalette(PipelineBaton* baton, bool val) { baton->pngPalette = val; }
int PipelineBaton_GetPngQuality(PipelineBaton* baton) { return baton->pngQuality; }
void PipelineBaton_SetPngQuality(PipelineBaton* baton, int val) { baton->pngQuality = val; }
int PipelineBaton_GetPngEffort(PipelineBaton* baton) { return baton->pngEffort; }
void PipelineBaton_SetPngEffort(PipelineBaton* baton, int val) { baton->pngEffort = val; }
int PipelineBaton_GetPngBitdepth(PipelineBaton* baton) { return baton->pngBitdepth; }
void PipelineBaton_SetPngBitdepth(PipelineBaton* baton, int val) { baton->pngBitdepth = val; }
double PipelineBaton_GetPngDither(PipelineBaton* baton) { return baton->pngDither; }
void PipelineBaton_SetPngDither(PipelineBaton* baton, double val) { baton->pngDither = val; }
int PipelineBaton_GetJp2Quality(PipelineBaton* baton) { return baton->jp2Quality; }
void PipelineBaton_SetJp2Quality(PipelineBaton* baton, int val) { baton->jp2Quality = val; }
bool PipelineBaton_GetJp2Lossless(PipelineBaton* baton) { return baton->jp2Lossless; }
void PipelineBaton_SetJp2Lossless(PipelineBaton* baton, bool val) { baton->jp2Lossless = val; }
int PipelineBaton_GetJp2TileHeight(PipelineBaton* baton) { return baton->jp2TileHeight; }
void PipelineBaton_SetJp2TileHeight(PipelineBaton* baton, int val) { baton->jp2TileHeight = val; }
int PipelineBaton_GetJp2TileWidth(PipelineBaton* baton) { return baton->jp2TileWidth; }
void PipelineBaton_SetJp2TileWidth(PipelineBaton* baton, int val) { baton->jp2TileWidth = val; }
const char* PipelineBaton_GetJp2ChromaSubsampling(PipelineBaton* baton) { return baton->jp2ChromaSubsampling.c_str(); }
void PipelineBaton_SetJp2ChromaSubsampling(PipelineBaton* baton, const char* val) { baton->jp2ChromaSubsampling = val; }
int PipelineBaton_GetWebpQuality(PipelineBaton* baton) { return baton->webpQuality; }
void PipelineBaton_SetWebpQuality(PipelineBaton* baton, int val) { baton->webpQuality = val; }
int PipelineBaton_GetWebpAlphaQuality(PipelineBaton* baton) { return baton->webpAlphaQuality; }
void PipelineBaton_SetWebpAlphaQuality(PipelineBaton* baton, int val) { baton->webpAlphaQuality = val; }
bool PipelineBaton_GetWebpNearLossless(PipelineBaton* baton) { return baton->webpNearLossless; }
void PipelineBaton_SetWebpNearLossless(PipelineBaton* baton, bool val) { baton->webpNearLossless = val; }
bool PipelineBaton_GetWebpLossless(PipelineBaton* baton) { return baton->webpLossless; }
void PipelineBaton_SetWebpLossless(PipelineBaton* baton, bool val) { baton->webpLossless = val; }
bool PipelineBaton_GetWebpSmartSubsample(PipelineBaton* baton) { return baton->webpSmartSubsample; }
void PipelineBaton_SetWebpSmartSubsample(PipelineBaton* baton, bool val) { baton->webpSmartSubsample = val; }
int PipelineBaton_GetWebpEffort(PipelineBaton* baton) { return baton->webpEffort; }
void PipelineBaton_SetWebpEffort(PipelineBaton* baton, int val) { baton->webpEffort = val; }
int PipelineBaton_GetGifBitdepth(PipelineBaton* baton) { return baton->gifBitdepth; }
void PipelineBaton_SetGifBitdepth(PipelineBaton* baton, int val) { baton->gifBitdepth = val; }
int PipelineBaton_GetGifEffort(PipelineBaton* baton) { return baton->gifEffort; }
void PipelineBaton_SetGifEffort(PipelineBaton* baton, int val) { baton->gifEffort = val; }
double PipelineBaton_GetGifDither(PipelineBaton* baton) { return baton->gifDither; }
void PipelineBaton_SetGifDither(PipelineBaton* baton, double val) { baton->gifDither = val; }
int PipelineBaton_GetTiffQuality(PipelineBaton* baton) { return baton->tiffQuality; }
void PipelineBaton_SetTiffQuality(PipelineBaton* baton, int val) { baton->tiffQuality = val; }
VipsForeignTiffCompression PipelineBaton_GetTiffCompression(PipelineBaton* baton) { return baton->tiffCompression; }
void PipelineBaton_SetTiffCompression(PipelineBaton* baton, VipsForeignTiffCompression val) { baton->tiffCompression = val; }
VipsForeignTiffPredictor PipelineBaton_GetTiffPredictor(PipelineBaton* baton) { return baton->tiffPredictor; }
void PipelineBaton_SetTiffPredictor(PipelineBaton* baton, VipsForeignTiffPredictor val) { baton->tiffPredictor = val; }
bool PipelineBaton_GetTiffPyramid(PipelineBaton* baton) { return baton->tiffPyramid; }
void PipelineBaton_SetTiffPyramid(PipelineBaton* baton, bool val) { baton->tiffPyramid = val; }
int PipelineBaton_GetTiffBitdepth(PipelineBaton* baton) { return baton->tiffBitdepth; }
void PipelineBaton_SetTiffBitdepth(PipelineBaton* baton, int val) { baton->tiffBitdepth = val; }
bool PipelineBaton_GetTiffTile(PipelineBaton* baton) { return baton->tiffTile; }
void PipelineBaton_SetTiffTile(PipelineBaton* baton, bool val) { baton->tiffTile = val; }
int PipelineBaton_GetTiffTileHeight(PipelineBaton* baton) { return baton->tiffTileHeight; }
void PipelineBaton_SetTiffTileHeight(PipelineBaton* baton, int val) { baton->tiffTileHeight = val; }
int PipelineBaton_GetTiffTileWidth(PipelineBaton* baton) { return baton->tiffTileWidth; }
void PipelineBaton_SetTiffTileWidth(PipelineBaton* baton, int val) { baton->tiffTileWidth = val; }
double PipelineBaton_GetTiffXres(PipelineBaton* baton) { return baton->tiffXres; }
void PipelineBaton_SetTiffXres(PipelineBaton* baton, double val) { baton->tiffXres = val; }
double PipelineBaton_GetTiffYres(PipelineBaton* baton) { return baton->tiffYres; }
void PipelineBaton_SetTiffYres(PipelineBaton* baton, double val) { baton->tiffYres = val; }
VipsForeignTiffResunit PipelineBaton_GetTiffResolutionUnit(PipelineBaton* baton) { return baton->tiffResolutionUnit; }
void PipelineBaton_SetTiffResolutionUnit(PipelineBaton* baton, VipsForeignTiffResunit val) { baton->tiffResolutionUnit = val; }
int PipelineBaton_GetHeifQuality(PipelineBaton* baton) { return baton->heifQuality; }
void PipelineBaton_SetHeifQuality(PipelineBaton* baton, int val) { baton->heifQuality = val; }
VipsForeignHeifCompression PipelineBaton_GetHeifCompression(PipelineBaton* baton) { return baton->heifCompression; }
void PipelineBaton_SetHeifCompression(PipelineBaton* baton, VipsForeignHeifCompression val) { baton->heifCompression = val; }
int PipelineBaton_GetHeifEffort(PipelineBaton* baton) { return baton->heifEffort; }
void PipelineBaton_SetHeifEffort(PipelineBaton* baton, int val) { baton->heifEffort = val; }
const char* PipelineBaton_GetHeifChromaSubsampling(PipelineBaton* baton) { return baton->heifChromaSubsampling.c_str(); }
void PipelineBaton_SetHeifChromaSubsampling(PipelineBaton* baton, const char* val) { baton->heifChromaSubsampling = val; }
bool PipelineBaton_GetHeifLossless(PipelineBaton* baton) { return baton->heifLossless; }
void PipelineBaton_SetHeifLossless(PipelineBaton* baton, bool val) { baton->heifLossless = val; }
VipsBandFormat PipelineBaton_GetRawDepth(PipelineBaton* baton) { return baton->rawDepth; }
void PipelineBaton_SetRawDepth(PipelineBaton* baton, VipsBandFormat val) { baton->rawDepth = val; }
const char* PipelineBaton_GetErr(PipelineBaton* baton) { return baton->err.c_str(); }
void PipelineBaton_SetErr(PipelineBaton* baton, const char* val) { baton->err = val; }
bool PipelineBaton_GetWithMetadata(PipelineBaton* baton) { return baton->withMetadata; }
void PipelineBaton_SetWithMetadata(PipelineBaton* baton, bool val) { baton->withMetadata = val; }
int PipelineBaton_GetWithMetadataOrientation(PipelineBaton* baton) { return baton->withMetadataOrientation; }
void PipelineBaton_SetWithMetadataOrientation(PipelineBaton* baton, int val) { baton->withMetadataOrientation = val; }
double PipelineBaton_GetWithMetadataDensity(PipelineBaton* baton) { return baton->withMetadataDensity; }
void PipelineBaton_SetWithMetadataDensity(PipelineBaton* baton, double val) { baton->withMetadataDensity = val; }
const char* PipelineBaton_GetWithMetadataIcc(PipelineBaton* baton) { return baton->withMetadataIcc.c_str(); }
void PipelineBaton_SetWithMetadataIcc(PipelineBaton* baton, const char* val) { baton->withMetadataIcc = val; }
std::unordered_map<std::string, std::string> PipelineBaton_GetWithMetadataStrs(PipelineBaton* baton) { return baton->withMetadataStrs; }
void PipelineBaton_SetWithMetadataStrs(PipelineBaton* baton, std::unordered_map<std::string, std::string> val) { baton->withMetadataStrs = val; }
int PipelineBaton_GetTimeoutSeconds(PipelineBaton* baton) { return baton->timeoutSeconds; }
void PipelineBaton_SetTimeoutSeconds(PipelineBaton* baton, int val) { baton->timeoutSeconds = val; }
double* PipelineBaton_GetConvKernel(PipelineBaton* baton) { return baton->convKernel.get(); }
void PipelineBaton_SetConvKernel(PipelineBaton* baton, std::unique_ptr<double[]> val) { baton->convKernel = std::move(val); }
int PipelineBaton_GetConvKernelWidth(PipelineBaton* baton) { return baton->convKernelWidth; }
void PipelineBaton_SetConvKernelWidth(PipelineBaton* baton, int val) { baton->convKernelWidth = val; }
int PipelineBaton_GetConvKernelHeight(PipelineBaton* baton) { return baton->convKernelHeight; }
void PipelineBaton_SetConvKernelHeight(PipelineBaton* baton, int val) { baton->convKernelHeight = val; }
double PipelineBaton_GetConvKernelScale(PipelineBaton* baton) { return baton->convKernelScale; }
void PipelineBaton_SetConvKernelScale(PipelineBaton* baton, double val) { baton->convKernelScale = val; }
double PipelineBaton_GetConvKernelOffset(PipelineBaton* baton) { return baton->convKernelOffset; }
void PipelineBaton_SetConvKernelOffset(PipelineBaton* baton, double val) { baton->convKernelOffset = val; }
InputDescriptor* PipelineBaton_GetBoolean(PipelineBaton* baton) { return baton->boolean; }
void PipelineBaton_SetBoolean(PipelineBaton* baton, InputDescriptor* val) { baton->boolean = val; }
VipsOperationBoolean PipelineBaton_GetBooleanOp(PipelineBaton* baton) { return baton->booleanOp; }
void PipelineBaton_SetBooleanOp(PipelineBaton* baton, VipsOperationBoolean val) { baton->booleanOp = val; }
VipsOperationBoolean PipelineBaton_GetBandBoolOp(PipelineBaton* baton) { return baton->bandBoolOp; }
void PipelineBaton_SetBandBoolOp(PipelineBaton* baton, VipsOperationBoolean val) { baton->bandBoolOp = val; }
int PipelineBaton_GetExtractChannel(PipelineBaton* baton) { return baton->extractChannel; }
void PipelineBaton_SetExtractChannel(PipelineBaton* baton, int val) { baton->extractChannel = val; }
bool PipelineBaton_GetRemoveAlpha(PipelineBaton* baton) { return baton->removeAlpha; }
void PipelineBaton_SetRemoveAlpha(PipelineBaton* baton, bool val) { baton->removeAlpha = val; }
double PipelineBaton_GetEnsureAlpha(PipelineBaton* baton) { return baton->ensureAlpha; }
void PipelineBaton_SetEnsureAlpha(PipelineBaton* baton, double val) { baton->ensureAlpha = val; }
VipsInterpretation PipelineBaton_GetColourspaceInput(PipelineBaton* baton) { return baton->colourspaceInput; }
void PipelineBaton_SetColourspaceInput(PipelineBaton* baton, VipsInterpretation val) { baton->colourspaceInput = val; }
VipsInterpretation PipelineBaton_GetColourspace(PipelineBaton* baton) { return baton->colourspace; }
void PipelineBaton_SetColourspace(PipelineBaton* baton, VipsInterpretation val) { baton->colourspace = val; }
std::vector<int> PipelineBaton_GetDelay(PipelineBaton* baton) { return baton->delay; }
void PipelineBaton_SetDelay(PipelineBaton* baton, std::vector<int> val) { baton->delay = val; }
int PipelineBaton_GetLoop(PipelineBaton* baton) { return baton->loop; }
void PipelineBaton_SetLoop(PipelineBaton* baton, int val) { baton->loop = val; }
int PipelineBaton_GetTileSize(PipelineBaton* baton) { return baton->tileSize; }
void PipelineBaton_SetTileSize(PipelineBaton* baton, int val) { baton->tileSize = val; }
int PipelineBaton_GetTileOverlap(PipelineBaton* baton) { return baton->tileOverlap; }
void PipelineBaton_SetTileOverlap(PipelineBaton* baton, int val) { baton->tileOverlap = val; }
VipsForeignDzContainer PipelineBaton_GetTileContainer(PipelineBaton* baton) { return baton->tileContainer; }
void PipelineBaton_SetTileContainer(PipelineBaton* baton, VipsForeignDzContainer val) { baton->tileContainer = val; }
VipsForeignDzLayout PipelineBaton_GetTileLayout(PipelineBaton* baton) { return baton->tileLayout; }
void PipelineBaton_SetTileLayout(PipelineBaton* baton, VipsForeignDzLayout val) { baton->tileLayout = val; }
const char* PipelineBaton_GetTileFormat(PipelineBaton* baton) { return baton->tileFormat.c_str(); }
void PipelineBaton_SetTileFormat(PipelineBaton* baton, const char* val) { baton->tileFormat = val; }
int PipelineBaton_GetTileAngle(PipelineBaton* baton) { return baton->tileAngle; }
void PipelineBaton_SetTileAngle(PipelineBaton* baton, int val) { baton->tileAngle = val; }
double* PipelineBaton_GetTileBackground(PipelineBaton* baton) { return baton->tileBackground.data(); }
void PipelineBaton_SetTileBackground(PipelineBaton* baton, std::vector<double> val) { baton->tileBackground = val; }
int PipelineBaton_GetTileSkipBlanks(PipelineBaton* baton) { return baton->tileSkipBlanks; }
void PipelineBaton_SetTileSkipBlanks(PipelineBaton* baton, int val) { baton->tileSkipBlanks = val; }
VipsForeignDzDepth PipelineBaton_GetTileDepth(PipelineBaton* baton) { return baton->tileDepth; }
void PipelineBaton_SetTileDepth(PipelineBaton* baton, VipsForeignDzDepth val) { baton->tileDepth = val; }
const char* PipelineBaton_GetTileId(PipelineBaton* baton) { return baton->tileId.c_str(); }
void PipelineBaton_SetTileId(PipelineBaton* baton, const char* val) { baton->tileId = val; }
double* PipelineBaton_GetRecombMatrix(PipelineBaton* baton) { return baton->recombMatrix.get(); }
void PipelineBaton_SetRecombMatrix(PipelineBaton* baton, std::unique_ptr<double[]> val) { baton->recombMatrix = std::move(val); }

void PipelineBaton_Composite_PushBack(PipelineBaton* baton, Composite * value) { baton->composite.push_back(value); }
void PipelineBaton_JoinChannelIn_PushBack(PipelineBaton* baton, InputDescriptor * value) { baton->joinChannelIn.push_back(value); }
void PipelineBaton_ResizeBackground_PushBack(PipelineBaton* baton, double value) { baton->resizeBackground.push_back(value); }
void PipelineBaton_FlattenBackground_PushBack(PipelineBaton* baton, double value) { baton->flattenBackground.push_back(value); }
void PipelineBaton_RotationBackground_PushBack(PipelineBaton* baton, double value) { baton->rotationBackground.push_back(value); }
void PipelineBaton_ExtendBackground_PushBack(PipelineBaton* baton, double value) { baton->extendBackground.push_back(value); }
void PipelineBaton_AffineMatrix_PushBack(PipelineBaton* baton, double value) { baton->affineMatrix.push_back(value); }
void PipelineBaton_AffineBackground_PushBack(PipelineBaton* baton, double value) { baton->affineBackground.push_back(value); }
void PipelineBaton_Delay_PushBack(PipelineBaton* baton, int value) { baton->delay.push_back(value); }
void PipelineBaton_TileBackground_PushBack(PipelineBaton* baton, double value) { baton->tileBackground.push_back(value); }

void PipelineBaton_WithMetadataStrs_Insert(PipelineBaton* baton, std::pair<std::string, std::string> value) { baton->withMetadataStrs.insert(value); }
