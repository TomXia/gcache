#include <cassert>
#include <cstdint>
#include <iostream>
#include <ostream>
#include <fstream>
#include <string>
#include <stdexcept>
#include <bits/stdc++.h>

#include "gcache/arc_cache.h"
#include "util.h"

using namespace gcache;

enum method_e {
    BASELINE,
    SLOPE,
    SHARDS,
};

enum workload_e {
    TRACE,
    RANDOM,
    SEQ,
};

class MRC {
    public:
        MRC(enum method_e method, uint32_t sampling_rate, uint32_t min_size, uint32_t max_size);
        enum method_e method() const {return _method;}
        uint32_t sampling_rate() const {return _sampling_rate;}
        uint32_t min_size() const {return _min_size;}
        uint32_t max_size() const {return _max_size;}
        uint64_t get_time() const {return rdtsc();}


        void save_mrc(std::string filename);
        void construct_mrc();
        void construct_baseline_mrc();
        void construct_slope_mrc();
        void construct_shards_mrc();

        void set_method(enum method_e method) {_method = method;}
        void set_sampling_rate(uint32_t sampling_rate) {_sampling_rate = sampling_rate;}
        void set_min_size(uint32_t min_size) {_min_size = min_size;}
        void set_max_size(uint32_t max_size) {_max_size = max_size;}

        void trace_workload(ARC_cache& cache, std::string filename);
        void random_workload(ARC_cache& cache, std::string filename="\0");
        void seq_workload(ARC_cache& cache, std::string filename="\0");

    private:
        enum method_e _method;
        uint32_t _sampling_rate;
        uint32_t _min_size;
        uint32_t _max_size;

        std::map <uint32_t, float> mrc_stats;
        std::map <enum workload_e, void (MRC::*)(ARC_cache&, std::string)> workload_map;
};
