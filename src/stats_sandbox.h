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

#ifndef SRC_STATS_SANDBOX_H_
#define SRC_STATS_SANDBOX_H_

#include <string>
#include <vector>

struct InputDescriptor;

struct ChannelStats {
  // stats per channel
  int min;
  int max;
  double sum;
  double squaresSum;
  double mean;
  double stdev;
  int minX;
  int minY;
  int maxX;
  int maxY;

  ChannelStats(int minVal, int maxVal, double sumVal, double squaresSumVal,
    double meanVal, double stdevVal, int minXVal, int minYVal, int maxXVal, int maxYVal):
    min(minVal), max(maxVal), sum(sumVal), squaresSum(squaresSumVal),
    mean(meanVal), stdev(stdevVal), minX(minXVal), minY(minYVal), maxX(maxXVal), maxY(maxYVal) {}
};

struct StatsBaton {
  // Input
  InputDescriptor *input;

  // Output
  std::vector<ChannelStats> channelStats;
  bool isOpaque;
  double entropy;
  double sharpness;
  int dominantRed;
  int dominantGreen;
  int dominantBlue;

  std::string err;

  StatsBaton():
    input(nullptr),
    isOpaque(true),
    entropy(0.0),
    sharpness(0.0),
    dominantRed(0),
    dominantGreen(0),
    dominantBlue(0)
    {}
};

extern "C" {
  void StatsWorkerExecute(StatsBaton* baton);

  StatsBaton* CreateStatsBaton();
  void DestroyStatsBaton(StatsBaton* baton);

  InputDescriptor* StatsBaton_GetInput(StatsBaton* baton);
  void StatsBaton_SetInput(StatsBaton* baton, InputDescriptor* val);
  ChannelStats* StatsBaton_GetChannelStats(StatsBaton* baton);
  void StatsBaton_SetChannelStats(StatsBaton* baton, std::vector<ChannelStats> val);
  bool StatsBaton_GetIsOpaque(StatsBaton* baton);
  void StatsBaton_SetIsOpaque(StatsBaton* baton, bool val);
  double StatsBaton_GetEntropy(StatsBaton* baton);
  void StatsBaton_SetEntropy(StatsBaton* baton, double val);
  double StatsBaton_GetSharpness(StatsBaton* baton);
  void StatsBaton_SetSharpness(StatsBaton* baton, double val);
  int StatsBaton_GetDominantRed(StatsBaton* baton);
  void StatsBaton_SetDominantRed(StatsBaton* baton, int val);
  int StatsBaton_GetDominantGreen(StatsBaton* baton);
  void StatsBaton_SetDominantGreen(StatsBaton* baton, int val);
  int StatsBaton_GetDominantBlue(StatsBaton* baton);
  void StatsBaton_SetDominantBlue(StatsBaton* baton, int val);
  const char* StatsBaton_GetErr(StatsBaton* baton);
  void StatsBaton_SetErr(StatsBaton* baton, const char* val);

  size_t StatsBaton_GetChannelStats_Size(StatsBaton* baton);
  bool StatsBaton_GetChannelStats_Empty(StatsBaton* baton);

}

#endif  // SRC_STATS_SANDBOX_H_
