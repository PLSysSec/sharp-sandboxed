#include "metadata_sandbox.h"
#include "common_sandbox.h"

#include <atomic>

void MetadataWorkerExecute(MetadataBaton* baton) {
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
        MetadataDimension val {width, height};
        baton->levels.push_back(val);
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

MetadataBaton* CreateMetadataBaton() {
  return new MetadataBaton;
}

void DestroyMetadataBaton(MetadataBaton* baton) {
  delete baton->input;
  delete baton;
}

sharp::InputDescriptor* MetadataBaton_GetInput(MetadataBaton* baton) { return baton->input; }
void MetadataBaton_SetInput(MetadataBaton* baton, sharp::InputDescriptor* val) { baton->input = val; }
const char* MetadataBaton_GetFormat(MetadataBaton* baton) { return baton->format.c_str(); }
void MetadataBaton_SetFormat(MetadataBaton* baton, const char* val) { baton->format = val; }
int MetadataBaton_GetWidth(MetadataBaton* baton) { return baton->width; }
void MetadataBaton_SetWidth(MetadataBaton* baton, int val) { baton->width = val; }
int MetadataBaton_GetHeight(MetadataBaton* baton) { return baton->height; }
void MetadataBaton_SetHeight(MetadataBaton* baton, int val) { baton->height = val; }
const char* MetadataBaton_GetSpace(MetadataBaton* baton) { return baton->space.c_str(); }
void MetadataBaton_SetSpace(MetadataBaton* baton, const char* val) { baton->space = val; }
int MetadataBaton_GetChannels(MetadataBaton* baton) { return baton->channels; }
void MetadataBaton_SetChannels(MetadataBaton* baton, int val) { baton->channels = val; }
const char* MetadataBaton_GetDepth(MetadataBaton* baton) { return baton->depth.c_str(); }
void MetadataBaton_SetDepth(MetadataBaton* baton, const char* val) { baton->depth = val; }
int MetadataBaton_GetDensity(MetadataBaton* baton) { return baton->density; }
void MetadataBaton_SetDensity(MetadataBaton* baton, int val) { baton->density = val; }
const char* MetadataBaton_GetChromaSubsampling(MetadataBaton* baton) { return baton->chromaSubsampling.c_str(); }
void MetadataBaton_SetChromaSubsampling(MetadataBaton* baton, const char* val) { baton->chromaSubsampling = val; }
bool MetadataBaton_GetIsProgressive(MetadataBaton* baton) { return baton->isProgressive; }
void MetadataBaton_SetIsProgressive(MetadataBaton* baton, bool val) { baton->isProgressive = val; }
int MetadataBaton_GetPaletteBitDepth(MetadataBaton* baton) { return baton->paletteBitDepth; }
void MetadataBaton_SetPaletteBitDepth(MetadataBaton* baton, int val) { baton->paletteBitDepth = val; }
int MetadataBaton_GetPages(MetadataBaton* baton) { return baton->pages; }
void MetadataBaton_SetPages(MetadataBaton* baton, int val) { baton->pages = val; }
int MetadataBaton_GetPageHeight(MetadataBaton* baton) { return baton->pageHeight; }
void MetadataBaton_SetPageHeight(MetadataBaton* baton, int val) { baton->pageHeight = val; }
int MetadataBaton_GetLoop(MetadataBaton* baton) { return baton->loop; }
void MetadataBaton_SetLoop(MetadataBaton* baton, int val) { baton->loop = val; }
int* MetadataBaton_GetDelay(MetadataBaton* baton) { return baton->delay.data(); }
void MetadataBaton_SetDelay(MetadataBaton* baton, std::vector<int> val) { baton->delay = val; }
int MetadataBaton_GetPagePrimary(MetadataBaton* baton) { return baton->pagePrimary; }
void MetadataBaton_SetPagePrimary(MetadataBaton* baton, int val) { baton->pagePrimary = val; }
const char* MetadataBaton_GetCompression(MetadataBaton* baton) { return baton->compression.c_str(); }
void MetadataBaton_SetCompression(MetadataBaton* baton, const char* val) { baton->compression = val; }
const char* MetadataBaton_GetResolutionUnit(MetadataBaton* baton) { return baton->resolutionUnit.c_str(); }
void MetadataBaton_SetResolutionUnit(MetadataBaton* baton, const char* val) { baton->resolutionUnit = val; }
MetadataDimension* MetadataBaton_GetLevels(MetadataBaton* baton) { return baton->levels.data(); }
void MetadataBaton_SetLevels(MetadataBaton* baton, std::vector<MetadataDimension> val) { baton->levels = val; }
int MetadataBaton_GetSubifds(MetadataBaton* baton) { return baton->subifds; }
void MetadataBaton_SetSubifds(MetadataBaton* baton, int val) { baton->subifds = val; }
double* MetadataBaton_GetBackground(MetadataBaton* baton) { return baton->background.data(); }
void MetadataBaton_SetBackground(MetadataBaton* baton, std::vector<double> val) { baton->background = val; }
bool MetadataBaton_GetHasProfile(MetadataBaton* baton) { return baton->hasProfile; }
void MetadataBaton_SetHasProfile(MetadataBaton* baton, bool val) { baton->hasProfile = val; }
bool MetadataBaton_GetHasAlpha(MetadataBaton* baton) { return baton->hasAlpha; }
void MetadataBaton_SetHasAlpha(MetadataBaton* baton, bool val) { baton->hasAlpha = val; }
int MetadataBaton_GetOrientation(MetadataBaton* baton) { return baton->orientation; }
void MetadataBaton_SetOrientation(MetadataBaton* baton, int val) { baton->orientation = val; }
char* MetadataBaton_GetExif(MetadataBaton* baton) { return baton->exif; }
void MetadataBaton_SetExif(MetadataBaton* baton, char* val) { baton->exif = val; }
size_t MetadataBaton_GetExifLength(MetadataBaton* baton) { return baton->exifLength; }
void MetadataBaton_SetExifLength(MetadataBaton* baton, size_t val) { baton->exifLength = val; }
char* MetadataBaton_GetIcc(MetadataBaton* baton) { return baton->icc; }
void MetadataBaton_SetIcc(MetadataBaton* baton, char* val) { baton->icc = val; }
size_t MetadataBaton_GetIccLength(MetadataBaton* baton) { return baton->iccLength; }
void MetadataBaton_SetIccLength(MetadataBaton* baton, size_t val) { baton->iccLength = val; }
char* MetadataBaton_GetIptc(MetadataBaton* baton) { return baton->iptc; }
void MetadataBaton_SetIptc(MetadataBaton* baton, char* val) { baton->iptc = val; }
size_t MetadataBaton_GetIptcLength(MetadataBaton* baton) { return baton->iptcLength; }
void MetadataBaton_SetIptcLength(MetadataBaton* baton, size_t val) { baton->iptcLength = val; }
char* MetadataBaton_GetXmp(MetadataBaton* baton) { return baton->xmp; }
void MetadataBaton_SetXmp(MetadataBaton* baton, char* val) { baton->xmp = val; }
size_t MetadataBaton_GetXmpLength(MetadataBaton* baton) { return baton->xmpLength; }
void MetadataBaton_SetXmpLength(MetadataBaton* baton, size_t val) { baton->xmpLength = val; }
char* MetadataBaton_GetTifftagPhotoshop(MetadataBaton* baton) { return baton->tifftagPhotoshop; }
void MetadataBaton_SetTifftagPhotoshop(MetadataBaton* baton, char* val) { baton->tifftagPhotoshop = val; }
size_t MetadataBaton_GetTifftagPhotoshopLength(MetadataBaton* baton) { return baton->tifftagPhotoshopLength; }
void MetadataBaton_SetTifftagPhotoshopLength(MetadataBaton* baton, size_t val) { baton->tifftagPhotoshopLength = val; }
const char* MetadataBaton_GetErr(MetadataBaton* baton) { return baton->err.c_str(); }
void MetadataBaton_SetErr(MetadataBaton* baton, const char* val) { baton->err = val; }

size_t MetadataBaton_GetDelay_Size(MetadataBaton* baton) { return baton->delay.size(); }
bool MetadataBaton_GetDelay_Empty(MetadataBaton* baton) { return baton->delay.empty(); }
size_t MetadataBaton_GetLevels_Size(MetadataBaton* baton) { return baton->levels.size(); }
bool MetadataBaton_GetLevels_Empty(MetadataBaton* baton) { return baton->levels.empty(); }
size_t MetadataBaton_GetBackground_Size(MetadataBaton* baton) { return baton->background.size(); }
bool MetadataBaton_GetBackground_Empty(MetadataBaton* baton) { return baton->background.empty(); }
