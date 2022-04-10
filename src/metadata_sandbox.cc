#include "metadata_sandbox.h"

#include <atomic>

void MetadataWorkerExecute(MetadataBaton* baton) {
  // Decrement queued task counter
  g_atomic_int_dec_and_test(&sharp::counterQueue);

  vips::VImage image;
  sharp::ImageType imageType = sharp::ImageType::UNKNOWN;
  try {
    std::tie(image, imageType) = OpenInput(baton->input);
  } catch (vips::VError const &err) {
    (baton->err).append(err.what());
  }
  if (imageType != sharp::ImageType::UNKNOWN) {
    // Image type
    baton->format = sharp::ImageTypeId(imageType);
    // VipsImage attributes
    baton->width = image.width();
    baton->height = image.height();
    baton->space = vips_enum_nick(VIPS_TYPE_INTERPRETATION, image.interpretation());
    baton->channels = image.bands();
    baton->depth = vips_enum_nick(VIPS_TYPE_BAND_FORMAT, image.format());
    if (sharp::HasDensity(image)) {
      baton->density = sharp::GetDensity(image);
    }
    if (image.get_typeof("jpeg-chroma-subsample") == VIPS_TYPE_REF_STRING) {
      baton->chromaSubsampling = image.get_string("jpeg-chroma-subsample");
    }
    if (image.get_typeof("interlaced") == G_TYPE_INT) {
      baton->isProgressive = image.get_int("interlaced") == 1;
    }
    if (image.get_typeof("palette-bit-depth") == G_TYPE_INT) {
      baton->paletteBitDepth = image.get_int("palette-bit-depth");
    }
    if (image.get_typeof(VIPS_META_N_PAGES) == G_TYPE_INT) {
      baton->pages = image.get_int(VIPS_META_N_PAGES);
    }
    if (image.get_typeof(VIPS_META_PAGE_HEIGHT) == G_TYPE_INT) {
      baton->pageHeight = image.get_int(VIPS_META_PAGE_HEIGHT);
    }
    if (image.get_typeof("loop") == G_TYPE_INT) {
      baton->loop = image.get_int("loop");
    }
    if (image.get_typeof("delay") == VIPS_TYPE_ARRAY_INT) {
      baton->delay = image.get_array_int("delay");
    }
    if (image.get_typeof("heif-primary") == G_TYPE_INT) {
      baton->pagePrimary = image.get_int("heif-primary");
    }
    if (image.get_typeof("heif-compression") == VIPS_TYPE_REF_STRING) {
      baton->compression = image.get_string("heif-compression");
    }
    if (image.get_typeof(VIPS_META_RESOLUTION_UNIT) == VIPS_TYPE_REF_STRING) {
      baton->resolutionUnit = image.get_string(VIPS_META_RESOLUTION_UNIT);
    }
    if (image.get_typeof("openslide.level-count") == VIPS_TYPE_REF_STRING) {
      int const levels = std::stoi(image.get_string("openslide.level-count"));
      for (int l = 0; l < levels; l++) {
        std::string prefix = "openslide.level[" + std::to_string(l) + "].";
        int const width = std::stoi(image.get_string((prefix + "width").data()));
        int const height = std::stoi(image.get_string((prefix + "height").data()));
        baton->levels.push_back(std::pair<int, int>(width, height));
      }
    }
    if (image.get_typeof(VIPS_META_N_SUBIFDS) == G_TYPE_INT) {
      baton->subifds = image.get_int(VIPS_META_N_SUBIFDS);
    }
    baton->hasProfile = sharp::HasProfile(image);
    if (image.get_typeof("background") == VIPS_TYPE_ARRAY_DOUBLE) {
      baton->background = image.get_array_double("background");
    }
    // Derived attributes
    baton->hasAlpha = sharp::HasAlpha(image);
    baton->orientation = sharp::ExifOrientation(image);
    // EXIF
    if (image.get_typeof(VIPS_META_EXIF_NAME) == VIPS_TYPE_BLOB) {
      size_t exifLength;
      void const *exif = image.get_blob(VIPS_META_EXIF_NAME, &exifLength);
      baton->exif = static_cast<char*>(g_malloc(exifLength));
      memcpy(baton->exif, exif, exifLength);
      baton->exifLength = exifLength;
    }
    // ICC profile
    if (image.get_typeof(VIPS_META_ICC_NAME) == VIPS_TYPE_BLOB) {
      size_t iccLength;
      void const *icc = image.get_blob(VIPS_META_ICC_NAME, &iccLength);
      baton->icc = static_cast<char*>(g_malloc(iccLength));
      memcpy(baton->icc, icc, iccLength);
      baton->iccLength = iccLength;
    }
    // IPTC
    if (image.get_typeof(VIPS_META_IPTC_NAME) == VIPS_TYPE_BLOB) {
      size_t iptcLength;
      void const *iptc = image.get_blob(VIPS_META_IPTC_NAME, &iptcLength);
      baton->iptc = static_cast<char *>(g_malloc(iptcLength));
      memcpy(baton->iptc, iptc, iptcLength);
      baton->iptcLength = iptcLength;
    }
    // XMP
    if (image.get_typeof(VIPS_META_XMP_NAME) == VIPS_TYPE_BLOB) {
      size_t xmpLength;
      void const *xmp = image.get_blob(VIPS_META_XMP_NAME, &xmpLength);
      baton->xmp = static_cast<char *>(g_malloc(xmpLength));
      memcpy(baton->xmp, xmp, xmpLength);
      baton->xmpLength = xmpLength;
    }
    // TIFFTAG_PHOTOSHOP
    if (image.get_typeof(VIPS_META_PHOTOSHOP_NAME) == VIPS_TYPE_BLOB) {
      size_t tifftagPhotoshopLength;
      void const *tifftagPhotoshop = image.get_blob(VIPS_META_PHOTOSHOP_NAME, &tifftagPhotoshopLength);
      baton->tifftagPhotoshop = static_cast<char *>(g_malloc(tifftagPhotoshopLength));
      memcpy(baton->tifftagPhotoshop, tifftagPhotoshop, tifftagPhotoshopLength);
      baton->tifftagPhotoshopLength = tifftagPhotoshopLength;
    }
  }

  // Clean up
  vips_error_clear();
  vips_thread_shutdown();
}
