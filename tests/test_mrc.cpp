#include "tests/test_mrc.h"

void
MRC::map_workloads() {
    workload_map[workload_e::TRACE] = &MRC::trace_workload;
    workload_map[workload_e::RANDOM] = &MRC::random_workload;
    workload_map[workload_e::SEQ] = &MRC::seq_workload;
}


MRC::MRC(enum method_e method, uint32_t sampling_rate, uint32_t min_size, uint32_t max_size):
    _method(method),
    _workload(workload_e::RANDOM),
    _tracefile("\0"),
    _sampling_rate(sampling_rate),
    _min_size(min_size),
    _max_size(max_size)
{
    map_workloads();
}

MRC::MRC(enum method_e method, enum workload_e workload, uint32_t sampling_rate, uint32_t min_size, uint32_t max_size):
    _method(method),
    _workload(workload),
    _tracefile("\0"),
    _sampling_rate(sampling_rate),
    _min_size(min_size),
    _max_size(max_size)
{
    map_workloads();
}

MRC::MRC(enum method_e method, enum workload_e workload, std::string filename, uint32_t sampling_rate, uint32_t min_size, uint32_t max_size):
    _method(method),
    _workload(workload),
    _tracefile(filename),
    _sampling_rate(sampling_rate),
    _min_size(min_size),
    _max_size(max_size)
{
    map_workloads();
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
        (this->*workload_map[_workload])(cache, _tracefile);
    }
}

void
MRC::construct_slope_mrc() {
    std::map<uint32_t, float> mrc_slopes;

    uint32_t max_iters = 1000;
    uint32_t num_iters = max_iters;

    //Warmup the mrc_slopes tree by calculating miss ratios for a few sizes
    for (uint32_t cache_size = _min_size; cache_size <= _max_size; cache_size *= 2) {
        ARC_cache cache(cache_size);
        (this->*workload_map[_workload])(cache, "\0");
    }

    std::tuple<uint32_t, float> prev_size_stats(0, 0);
    std::tuple<uint32_t, float> curr_size_stats(prev_size_stats);
    for (const auto& it: mrc_stats) {
        curr_size_stats = std::tuple<uint32_t, float>(it.first, it.second);
        if (std::get<0>(prev_size_stats) != 0) {
            mrc_slopes[std::get<0>(prev_size_stats)] = abs(std::get<1>(curr_size_stats) - std::get<1>(prev_size_stats))/(std::get<0>(curr_size_stats) - std::get<0>(prev_size_stats));
        }
        prev_size_stats = curr_size_stats;
    }

    float threshold = 0.001;
    auto it = mrc_slopes.begin();
    while (it != mrc_slopes.end()) {
       if (it->second > threshold) {
           auto upper_bound_stats = mrc_stats.upper_bound(it->first);
           auto cache_size = (it->first + upper_bound_stats->first)/2;
           ARC_cache cache(cache_size);
           (this->*workload_map[_workload])(cache, "\0");
           mrc_slopes[it->first] = abs(mrc_stats[cache_size] - mrc_stats[it->first])/(cache_size - it->first);
           mrc_slopes[upper_bound_stats->first] = (upper_bound_stats->second - mrc_stats[cache_size])/(upper_bound_stats->first - cache_size);
           num_iters--;
           if (num_iters == 0) {
               std::cout << "Max iters reached\n";
               break;
           }
       } else {
           it++;
       }
    }
}

void
MRC::construct_shards_mrc() {
    std::cout << "Not implemented\n";
    std::terminate();
}


void
MRC::trace_workload(ARC_cache &cache, std::string filename) {
    if (trace_requests.empty()) {
        parse_tracefile(filename, "bigtable");
    }
    play_tracefile(cache);

    mrc_stats[cache.capacity()] = cache.get_miss_rate();
}

void
MRC::seq_workload(ARC_cache &cache, std::string filename) {
    std::vector<uint32_t> reqs;
    uint32_t num_accesses = max_size()/2;
    for (uint32_t i = 0; i < num_accesses; ++i)
      reqs.emplace_back(i);

    auto iters = 4;
    for (int j = 0; j < iters; ++j) {
        for(auto i : reqs) {
          cache.access(i);
        }
    }

    mrc_stats[cache.capacity()] = cache.get_miss_rate();
}
 
 
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

void
MRC::parse_tracefile(std::string filename, std::string app) {
    std::cout << "Parsing Tracefile\n";
	std::ifstream file(filename);
	std::string line;
	int count = 0;
	bool skip_header = false;
	int rs_col = -1, offset_col = -1, fname_col = -1, app_col = -1;

	while (std::getline(file, line)) {
		std::stringstream ss(line);
		std::string cell;
		if (!skip_header) {
			skip_header = true;
			while (std::getline(ss, cell, ',')) {
				if (cell == "filename") {
				    fname_col = count;
				} else if (cell == "file_offset") {
				    offset_col = count;
				} else if (cell == "request_io_size_bytes") {
				    rs_col = count;
				} else if (cell == "application") {
				    app_col = count; 
				}
				count++;
			}
		}
		if ((rs_col < 0) || (offset_col < 0) || (fname_col < 0) || (app_col < -1)) {
			std::cout << "request io, offset or filename column not found in data!" << std::endl;
			std::cout << "request io size bytes column (" << rs_col << ", index)" << std::endl;
			std::cout << "offset size column (" << offset_col << ", index)" << std::endl;
			std::cout << "filename column (" << fname_col << ", index)" << std::endl;
			return ;

		}
		count = 0;
		uint32_t file_offset = 0;
		uint32_t rs = 0;
		uint32_t total_size = 0;
		std::string filename;
		bool skip_row = false; 
		while (std::getline(ss, cell, ',')) {
			if (count == app_col) {
				if (cell != app && (!app.empty())) {
					skip_row = true;
					break;
				} 
			} else if (count == fname_col) {
				filename = cell;
			} else if (count == rs_col) {
				rs = (uint32_t) std::atoi(cell.c_str());
			} else if(count == offset_col) {
				file_offset = (uint32_t) std::atoi(cell.c_str());
			}
			count++;
		}

		if (skip_row) {
			continue; 
		} else {
			if (!trace_requests.count(filename)) {
				trace_requests.insert(std::make_pair(filename,
					       std::make_tuple(
						       0xffffffff,
						       0)));
			}
			total_size = rs + file_offset;
			if (file_offset < std::get<0>(trace_requests[filename])) {
				std::get<0>(trace_requests[filename]) = file_offset;
			}
			if (total_size > std::get<1>(trace_requests[filename])) {
				std::get<1>(trace_requests[filename]) = total_size;
				std::cout << "new max rq size is " << total_size << std::endl;
			}
		}
	}
}

uint32_t
MRC::play_tracefile(ARC_cache &cache) {
	uint32_t prev_boundary = 0; 
	for (auto it = trace_requests.cbegin(); it != trace_requests.cend(); ++it) {
		if (it->first.empty())
			continue;
		uint32_t curr_min_offset = std::get<0>(it->second);
		uint32_t curr_max_size = std::get<1>(it->second);
		uint32_t boundary = ((curr_max_size - curr_min_offset) / block_size) + prev_boundary;
		for (uint32_t i = prev_boundary; i < boundary; i++){
			cache.access(i); 
			//std::cout << '\t' << i << std::endl;
		}
		prev_boundary = boundary; 
	}
	return prev_boundary; 
}


int main() {
    uint32_t min_size = 32;
    uint32_t max_size = 4096;
    method_e method = method_e::BASELINE;
    workload_e workload = workload_e::SEQ;
    uint32_t sampling_rate = 1;

    MRC mrc(method, workload, sampling_rate, min_size, max_size);

    mrc.construct_mrc();
    mrc.save_mrc("baseline.csv");

    mrc.set_method(method_e::SLOPE);
    mrc.construct_mrc();
    mrc.save_mrc("slope.csv");

    mrc.set_method(method_e::BASELINE);
    mrc.set_workload(workload_e::TRACE);
    mrc.set_tracefile("../thesios_traces/thesios-subset.csv");
    mrc.construct_mrc();
    mrc.save_mrc("trace.csv");

    return 0;
}
