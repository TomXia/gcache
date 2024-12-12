#include "tests/test_mrc.h"

MRC::MRC(enum method_e method, uint32_t sampling_rate, uint32_t min_size, uint32_t max_size):
    _method(method),
    _sampling_rate(sampling_rate),
    _min_size(min_size),
    _max_size(max_size)
{
    workload_map[workload_e::TRACE] = &MRC::trace_workload;
    workload_map[workload_e::RANDOM] = &MRC::random_workload;
    workload_map[workload_e::SEQ] = &MRC::seq_workload;
}


void
MRC::save_mrc(std::string filename) {
    std::ofstream fout(filename, std::ios::trunc | std::ios::out);
    fout << "Cache size,Miss Rate\n";
    fout << std::flush;
    for (const auto& [size, miss_rate]: mrc_stats)
        fout << size << "," << miss_rate << std::endl;
    fout << std::flush;
    fout.close();
    return;
}

void
MRC::construct_mrc() {
    mrc_stats.clear();
    if (method() == method_e::BASELINE) {
        construct_baseline_mrc();
    } else {
        construct_slope_mrc();
    }
}

void
MRC::construct_baseline_mrc() {
    uint32_t step_size = _min_size;
    for (uint32_t cache_size = _min_size; cache_size <= _max_size; cache_size += step_size) {
        ARC_cache cache(cache_size);
        (this->*workload_map[workload_e::RANDOM])(cache, "\0");
    }
}

void
MRC::construct_slope_mrc() {
    std::map<uint32_t, float> mrc_slopes;

    float threshold = 0.001;
    uint32_t num_iters = 100;

    //Warmup the mrc_slopes tree by calculating miss ratios for a few sizes
    for (uint32_t cache_size = _min_size; cache_size <= _max_size; cache_size *= 2) {
        ARC_cache cache(cache_size);
        (this->*workload_map[workload_e::RANDOM])(cache, "\0");
    }

    std::tuple<uint32_t, float> prev_size_stats(-1, -1);
    std::tuple<uint32_t, float> curr_size_stats(prev_size_stats);
    for (const auto& pair: mrc_stats) {
        curr_size_stats = std::tuple<uint32_t, float>(pair.first, pair.second);
        if (std::get<0>(prev_size_stats) != -1) {
            mrc_slopes[std::get<0>(prev_size_stats)] = (std::get<1>(curr_size_stats) - std::get<1>(prev_size_stats))/(std::get<0>(curr_size_stats) - std::get<0>(prev_size_stats));
        }
        prev_size_stats = curr_size_stats;
    }

    for (const auto& pair: mrc_slopes) {
       if (pair.second > threshold) {
           auto upper_bound_stats = mrc_stats.upper_bound(pair.first);
           auto cache_size = (pair.first + upper_bound_stats->first)/2;
           ARC_cache cache(cache_size);
           (this->*workload_map[workload_e::RANDOM])(cache, "\0");
           mrc_slopes[pair.first] = (mrc_stats[cache_size] - mrc_stats[pair.first])/(cache_size - pair.first);
           mrc_slopes[upper_bound_stats->first] = (upper_bound_stats->second - mrc_stats[cache_size])/(upper_bound_stats->first - cache_size);
           num_iters--;
           if (num_iters == 0) {
               std::cout << "Max iters reached\n";
               break;
           }
       }
    }

}

void
MRC::construct_shards_mrc() {
    std::cout << "Not implemented\n";
    std::terminate();
}


void
MRC::trace_workload(ARC_cache &cache, std::string filename) {}

void
MRC::seq_workload(ARC_cache &cache, std::string filename) {}
 
 
void
MRC::random_workload(ARC_cache &cache, std::string filename) {
    std::vector<uint32_t> reqs;

    uint32_t num_accesses = 4 * cache.capacity();
    uint32_t req_max = 8 * cache.capacity();
    for (uint32_t i = 0; i < num_accesses; ++i)
      reqs.emplace_back(rand() % req_max);

    for(auto i : reqs) {
      cache.access(i);
    }

    mrc_stats[cache.capacity()] = cache.get_miss_rate();
}


int main() {
    uint32_t min_size = 32;
    uint32_t max_size = 4096;
    method_e method = method_e::BASELINE;
    uint32_t sampling_rate = 1;

    MRC mrc(method, sampling_rate, min_size, max_size);

    mrc.construct_mrc();
    mrc.save_mrc("baseline.csv");

    mrc.set_method(method_e::SLOPE);
    mrc.construct_mrc();
    mrc.save_mrc("slope.csv");

    return 0;
}