#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>

#include "gcache/ghost_cache.h"
#include "gcache/node.h"
#include "util.h"

using namespace gcache;

#define NUM_ACCESS   (1024)
#define DEFAULT_MIN  (8)
#define DEFAULT_TICK (4)
#define DEFAULT_MAX  (256)
#define PLT_INTV     (128)
#define BLK_SIZE     (4096)
#define REQ_MAX      (256)

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

  for (uint32_t i = 0; i < NUM_ACCESS; ++i)
    reqs.emplace_back(rand() % REQ_MAX);

  for (auto i : reqs) access(i);

  return 0;
}