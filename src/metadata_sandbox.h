#ifndef SRC_METADATA_SANDBOX_H_
#define SRC_METADATA_SANDBOX_H_

#include <string>

#include "./common_sandbox.h"

struct MetadataBaton {
  // Input
  sharp::InputDescriptor *input;
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
  std::vector<std::pair<int, int>> levels;
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
}

#endif  // SRC_METADATA_SANDBOX_H_