#include <vips/vips8>

#include "stats_sandbox.h"


static const int STAT_MIN_INDEX = 0;
static const int STAT_MAX_INDEX = 1;
static const int STAT_SUM_INDEX = 2;
static const int STAT_SQ_SUM_INDEX = 3;
static const int STAT_MEAN_INDEX = 4;
static const int STAT_STDEV_INDEX = 5;
static const int STAT_MINX_INDEX = 6;
static const int STAT_MINY_INDEX = 7;
static const int STAT_MAXX_INDEX = 8;
static const int STAT_MAXY_INDEX = 9;

void StatsWorkerExecute(StatsBaton* baton) {
  vips::VImage image;
  sharp::ImageType imageType = sharp::ImageType::UNKNOWN;
  try {
    std::tie(image, imageType) = OpenInput(baton->input);
  } catch (vips::VError const &err) {
    (baton->err).append(err.what());
  }
  if (imageType != sharp::ImageType::UNKNOWN) {
    try {
      vips::VImage stats = image.stats();
      int const bands = image.bands();
      for (int b = 1; b <= bands; b++) {
        ChannelStats cStats(
          static_cast<int>(stats.getpoint(STAT_MIN_INDEX, b).front()),
          static_cast<int>(stats.getpoint(STAT_MAX_INDEX, b).front()),
          stats.getpoint(STAT_SUM_INDEX, b).front(),
          stats.getpoint(STAT_SQ_SUM_INDEX, b).front(),
          stats.getpoint(STAT_MEAN_INDEX, b).front(),
          stats.getpoint(STAT_STDEV_INDEX, b).front(),
          static_cast<int>(stats.getpoint(STAT_MINX_INDEX, b).front()),
          static_cast<int>(stats.getpoint(STAT_MINY_INDEX, b).front()),
          static_cast<int>(stats.getpoint(STAT_MAXX_INDEX, b).front()),
          static_cast<int>(stats.getpoint(STAT_MAXY_INDEX, b).front()));
        baton->channelStats.push_back(cStats);
      }
      // Image is not opaque when alpha layer is present and contains a non-mamixa value
      if (sharp::HasAlpha(image)) {
        double const minAlpha = static_cast<double>(stats.getpoint(STAT_MIN_INDEX, bands).front());
        if (minAlpha != sharp::MaximumImageAlpha(image.interpretation())) {
          baton->isOpaque = false;
        }
      }
      // Convert to greyscale
      vips::VImage greyscale = image.colourspace(VIPS_INTERPRETATION_B_W)[0];
      // Estimate entropy via histogram of greyscale value frequency
      baton->entropy = std::abs(greyscale.hist_find().hist_entropy());
      // Estimate sharpness via standard deviation of greyscale laplacian
      if (image.width() > 1 || image.height() > 1) {
        VImage laplacian = VImage::new_matrixv(3, 3,
          0.0,  1.0, 0.0,
          1.0, -4.0, 1.0,
          0.0,  1.0, 0.0);
        laplacian.set("scale", 9.0);
        baton->sharpness = greyscale.conv(laplacian).deviate();
      }
      // Most dominant sRGB colour via 4096-bin 3D histogram
      vips::VImage hist = sharp::RemoveAlpha(image)
        .colourspace(VIPS_INTERPRETATION_sRGB)
        .hist_find_ndim(VImage::option()->set("bins", 16));
      std::complex<double> maxpos = hist.maxpos();
      int const dx = static_cast<int>(std::real(maxpos));
      int const dy = static_cast<int>(std::imag(maxpos));
      std::vector<double> pel = hist(dx, dy);
      int const dz = std::distance(pel.begin(), std::find(pel.begin(), pel.end(), hist.max()));
      baton->dominantRed = dx * 16 + 8;
      baton->dominantGreen = dy * 16 + 8;
      baton->dominantBlue = dz * 16 + 8;
    } catch (vips::VError const &err) {
      (baton->err).append(err.what());
    }
  }

  // Clean up
  vips_error_clear();
  vips_thread_shutdown();
}

StatsBaton* CreateStatsBaton() {
  return new StatsBaton;
}

void DestroyStatsBaton(StatsBaton* baton) {
  delete baton->input;
  delete baton;
}
