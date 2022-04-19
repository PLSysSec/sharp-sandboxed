#ifndef SRC_METADATA_SANDBOX_H_
#define SRC_METADATA_SANDBOX_H_

#include <string>
#include <vector>

struct InputDescriptor;

struct MetadataDimension {
    int width;
    int height;
};
struct MetadataBaton {
  // Input
  InputDescriptor *input;
  // Output
  std::string format;
  int width;
  int height;
  std::string space;
  int channels;
  std::string depth;
  int density;
  std::string chromaSubsampling;
  bool isProgressive;
  int paletteBitDepth;
  int pages;
  int pageHeight;
  int loop;
  std::vector<int> delay;
  int pagePrimary;
  std::string compression;
  std::string resolutionUnit;
  std::vector<MetadataDimension> levels;
  int subifds;
  std::vector<double> background;
  bool hasProfile;
  bool hasAlpha;
  int orientation;
  char *exif;
  size_t exifLength;
  char *icc;
  size_t iccLength;
  char *iptc;
  size_t iptcLength;
  char *xmp;
  size_t xmpLength;
  char *tifftagPhotoshop;
  size_t tifftagPhotoshopLength;
  std::string err;

  MetadataBaton():
    input(nullptr),
    width(0),
    height(0),
    channels(0),
    density(0),
    isProgressive(false),
    paletteBitDepth(0),
    pages(0),
    pageHeight(0),
    loop(-1),
    pagePrimary(-1),
    subifds(0),
    hasProfile(false),
    hasAlpha(false),
    orientation(0),
    exif(nullptr),
    exifLength(0),
    icc(nullptr),
    iccLength(0),
    iptc(nullptr),
    iptcLength(0),
    xmp(nullptr),
    xmpLength(0),
    tifftagPhotoshop(nullptr),
    tifftagPhotoshopLength(0) {}
};

extern "C" {
  void MetadataWorkerExecute(MetadataBaton* baton);

  MetadataBaton* CreateMetadataBaton();
  void DestroyMetadataBaton(MetadataBaton* baton);

  InputDescriptor* MetadataBaton_GetInput(MetadataBaton* baton);
  void MetadataBaton_SetInput(MetadataBaton* baton, InputDescriptor* val);
  const char* MetadataBaton_GetFormat(MetadataBaton* baton);
  void MetadataBaton_SetFormat(MetadataBaton* baton, const char* val);
  int MetadataBaton_GetWidth(MetadataBaton* baton);
  void MetadataBaton_SetWidth(MetadataBaton* baton, int val);
  int MetadataBaton_GetHeight(MetadataBaton* baton);
  void MetadataBaton_SetHeight(MetadataBaton* baton, int val);
  const char* MetadataBaton_GetSpace(MetadataBaton* baton);
  void MetadataBaton_SetSpace(MetadataBaton* baton, const char* val);
  int MetadataBaton_GetChannels(MetadataBaton* baton);
  void MetadataBaton_SetChannels(MetadataBaton* baton, int val);
  const char* MetadataBaton_GetDepth(MetadataBaton* baton);
  void MetadataBaton_SetDepth(MetadataBaton* baton, const char* val);
  int MetadataBaton_GetDensity(MetadataBaton* baton);
  void MetadataBaton_SetDensity(MetadataBaton* baton, int val);
  const char* MetadataBaton_GetChromaSubsampling(MetadataBaton* baton);
  void MetadataBaton_SetChromaSubsampling(MetadataBaton* baton, const char* val);
  bool MetadataBaton_GetIsProgressive(MetadataBaton* baton);
  void MetadataBaton_SetIsProgressive(MetadataBaton* baton, bool val);
  int MetadataBaton_GetPaletteBitDepth(MetadataBaton* baton);
  void MetadataBaton_SetPaletteBitDepth(MetadataBaton* baton, int val);
  int MetadataBaton_GetPages(MetadataBaton* baton);
  void MetadataBaton_SetPages(MetadataBaton* baton, int val);
  int MetadataBaton_GetPageHeight(MetadataBaton* baton);
  void MetadataBaton_SetPageHeight(MetadataBaton* baton, int val);
  int MetadataBaton_GetLoop(MetadataBaton* baton);
  void MetadataBaton_SetLoop(MetadataBaton* baton, int val);
  int* MetadataBaton_GetDelay(MetadataBaton* baton);
  void MetadataBaton_SetDelay(MetadataBaton* baton, std::vector<int> val);
  int MetadataBaton_GetPagePrimary(MetadataBaton* baton);
  void MetadataBaton_SetPagePrimary(MetadataBaton* baton, int val);
  const char* MetadataBaton_GetCompression(MetadataBaton* baton);
  void MetadataBaton_SetCompression(MetadataBaton* baton, const char* val);
  const char* MetadataBaton_GetResolutionUnit(MetadataBaton* baton);
  void MetadataBaton_SetResolutionUnit(MetadataBaton* baton, const char* val);
  MetadataDimension* MetadataBaton_GetLevels(MetadataBaton* baton);
  void MetadataBaton_SetLevels(MetadataBaton* baton, std::vector<MetadataDimension> val);
  int MetadataBaton_GetSubifds(MetadataBaton* baton);
  void MetadataBaton_SetSubifds(MetadataBaton* baton, int val);
  double* MetadataBaton_GetBackground(MetadataBaton* baton);
  void MetadataBaton_SetBackground(MetadataBaton* baton, std::vector<double> val);
  bool MetadataBaton_GetHasProfile(MetadataBaton* baton);
  void MetadataBaton_SetHasProfile(MetadataBaton* baton, bool val);
  bool MetadataBaton_GetHasAlpha(MetadataBaton* baton);
  void MetadataBaton_SetHasAlpha(MetadataBaton* baton, bool val);
  int MetadataBaton_GetOrientation(MetadataBaton* baton);
  void MetadataBaton_SetOrientation(MetadataBaton* baton, int val);
  char* MetadataBaton_GetExif(MetadataBaton* baton);
  void MetadataBaton_SetExif(MetadataBaton* baton, char* val);
  size_t MetadataBaton_GetExifLength(MetadataBaton* baton);
  void MetadataBaton_SetExifLength(MetadataBaton* baton, size_t val);
  char* MetadataBaton_GetIcc(MetadataBaton* baton);
  void MetadataBaton_SetIcc(MetadataBaton* baton, char* val);
  size_t MetadataBaton_GetIccLength(MetadataBaton* baton);
  void MetadataBaton_SetIccLength(MetadataBaton* baton, size_t val);
  char* MetadataBaton_GetIptc(MetadataBaton* baton);
  void MetadataBaton_SetIptc(MetadataBaton* baton, char* val);
  size_t MetadataBaton_GetIptcLength(MetadataBaton* baton);
  void MetadataBaton_SetIptcLength(MetadataBaton* baton, size_t val);
  char* MetadataBaton_GetXmp(MetadataBaton* baton);
  void MetadataBaton_SetXmp(MetadataBaton* baton, char* val);
  size_t MetadataBaton_GetXmpLength(MetadataBaton* baton);
  void MetadataBaton_SetXmpLength(MetadataBaton* baton, size_t val);
  char* MetadataBaton_GetTifftagPhotoshop(MetadataBaton* baton);
  void MetadataBaton_SetTifftagPhotoshop(MetadataBaton* baton, char* val);
  size_t MetadataBaton_GetTifftagPhotoshopLength(MetadataBaton* baton);
  void MetadataBaton_SetTifftagPhotoshopLength(MetadataBaton* baton, size_t val);
  const char* MetadataBaton_GetErr(MetadataBaton* baton);
  void MetadataBaton_SetErr(MetadataBaton* baton, const char* val);

  size_t MetadataBaton_GetDelay_Size(MetadataBaton* baton);
  bool MetadataBaton_GetDelay_Empty(MetadataBaton* baton);
  size_t MetadataBaton_GetLevels_Size(MetadataBaton* baton);
  bool MetadataBaton_GetLevels_Empty(MetadataBaton* baton);
  size_t MetadataBaton_GetBackground_Size(MetadataBaton* baton);
  bool MetadataBaton_GetBackground_Empty(MetadataBaton* baton);
}

#endif  // SRC_METADATA_SANDBOX_H_