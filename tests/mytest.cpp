#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>

#include "gcache/ghost_cache.h"
#include "gcache/node.h"
#include "gcache/arc_cache.h"
#include "util.h"

using namespace gcache;

#define NUM_ACCESS   (8192)
#define DEFAULT_MIN  (8)
#define DEFAULT_TICK (4)
#define DEFAULT_MAX  (256)
#define PLT_INTV     (128)
#define BLK_SIZE     (4096)
#define REQ_MAX      (16384)

GhostCache<> target_cache(DEFAULT_TICK, DEFAULT_MIN, DEFAULT_MAX);

void warm()
{
  for(uint32_t i = 0; i < DEFAULT_MAX; ++i)
    target_cache.access(i);
  target_cache.reset_stat();
}

void access(uint32_t blk_id)
{
  static uint32_t counter = 0;
  target_cache.access(blk_id);

  ++counter;
  if(counter == PLT_INTV)
  {//generate plot
    for(uint32_t s = target_cache.get_min_size(); s <= target_cache.get_max_size(); s += DEFAULT_TICK)
      std::cout << s*BLK_SIZE/1024 << ",";
    std::cout << std::endl;
    for(uint32_t s = target_cache.get_min_size(); s <= target_cache.get_max_size(); s += DEFAULT_TICK)
      std::cout << target_cache.get_hit_rate(s) << ',';
    std::cout << std::endl;
  
    counter = 0;
  }
}

int main() {
  std::vector<uint32_t> reqs;
  std::vector<std::pair<uint32_t, float> > miss_rates;
  
  ARC_mrc cache = ARC_mrc(32, 4096, 32, 256);

  for (uint32_t i = 0; i < NUM_ACCESS; ++i)
    reqs.emplace_back(rand() % REQ_MAX);

  // for(uint32_t i = 0; i < 2048; ++i)
  // {//build T2
  //   cache.access(i);
  //   cache.access(i);
  // }
  // cache.reset_stat();
  for(auto i : reqs)
  {
    cache.access(i);
  }


  cache.get_miss_rates(miss_rates);

  for(auto i : miss_rates)
    std::cout << i.first << ' ' << i.second << std::endl;
  return 0;
}