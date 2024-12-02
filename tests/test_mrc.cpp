#include <cassert>
#include <cstdint>
#include <iostream>
#include <ostream>
#include <fstream>
#include <stdexcept>
#include <bits/stdc++.h>

#include "gcache/lru_cache.h"
#include "util.h"

using namespace gcache;

struct hash1 {
  uint32_t operator()(uint32_t x) const noexcept { return x + 1000; }
};

struct hash2 {
  uint32_t operator()(uint32_t x) const noexcept { return x; }
};

void print_stats(std::ofstream &fout, int cache_size, unsigned long ts0, unsigned long ts1, unsigned long ts2, unsigned long ts3) {
  fout << cache_size << ",";
  fout << (ts1 - ts0) << ",";
  fout << (ts1 - ts0) / (cache_size) << ",";
  fout << (ts2 - ts1) << ",";
  fout << (ts2 - ts1) / (cache_size) << ",";
  fout << (ts3 - ts2) << ",";
  fout << (ts3 - ts2) / (cache_size) << "\n";
  fout << std::flush;
}

void workload(LRUCache<uint32_t, uint32_t, hash2> &cache, int cache_size, std::ofstream &fout) {
  // filling the cache
  auto ts0 = rdtsc();
  for (int i = 0; i < cache_size; ++i) cache.insert(i);
  auto ts1 = rdtsc();

  // cache hit
  for (int i = 0; i < cache_size; ++i) cache.insert(i);
  auto ts2 = rdtsc();

  // cache miss
  for (int i = 0; i < cache_size; ++i) cache.insert(i + cache_size);
  auto ts3 = rdtsc();

  print_stats(fout, cache_size, ts0, ts1, ts2, ts3);
}

void mrc_sweep_exp(int start_size, int end_size, int inc) {
  int cache_size = start_size;
  std::string outfile = __func__;
  outfile.append(".csv");
  std::ofstream fout(outfile, std::ios::trunc | std::ios::out);
  fout << "Cache Size,Fill Time,Fill Rate (cycles/op),Hit Time, Hit Rate (cycles/op),Miss Time,Miss Rate(cycles/op)\n";
  fout << std::flush;
  while (cache_size < end_size) {
      LRUCache<uint32_t, uint32_t, hash2> cache;
      cache.init(cache_size);
      assert(cache.size() == 0);
      assert(cache.capacity() == cache_size);
      workload(cache, cache_size, fout);
      cache_size *= inc;
  }
  fout.close();
}

int main() {
  int start_size = 256 * 1024;
  int end_size = 256 * 1024 * 1024;
  int inc = 2;
  mrc_sweep_exp(start_size, end_size, inc);  // for performance
  return 0;
}
