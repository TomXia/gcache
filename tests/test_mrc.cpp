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
    _max_size(max_size),
    num_trace_played(0)
{
    map_workloads();
}

MRC::MRC(enum method_e method, enum workload_e workload, uint32_t sampling_rate, uint32_t min_size, uint32_t max_size):
    _method(method),
    _workload(workload),
    _tracefile("\0"),
    _sampling_rate(sampling_rate),
    _min_size(min_size),
    _max_size(max_size),
    num_trace_played(0)
{
    map_workloads();
}

MRC::MRC(enum method_e method, enum workload_e workload, std::string filename, uint32_t sampling_rate, uint32_t min_size, uint32_t max_size):
    _method(method),
    _workload(workload),
    _tracefile(filename),
    _sampling_rate(sampling_rate),
    _min_size(min_size),
    _max_size(max_size),
    num_trace_played(0)
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
    trace_requests.clear();
    if (method() == method_e::BASELINE) {
        construct_baseline_mrc();
    } else {
        construct_slope_mrc();
    }
}

void
MRC::construct_baseline_mrc() {
    uint32_t step_size = (_max_size - _min_size)/501;
    for (uint32_t cache_size = _min_size; cache_size <= _max_size; cache_size += step_size) {
        std::cout << "Size: " << cache_size << "/" << _max_size << std::endl;
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
        std::cout << "Size: " << cache_size << "/" << _max_size << std::endl;
        ARC_cache cache(cache_size);
        (this->*workload_map[_workload])(cache, _tracefile);
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

    float threshold = 0.01;
    auto it = mrc_slopes.begin();
    while (it != mrc_slopes.end()) {
       auto upper_bound_stats = mrc_stats.upper_bound(it->first);
       if ((it->second * (upper_bound_stats->first - it->first)) > threshold) {
           auto cache_size = (it->first + upper_bound_stats->first)/2;
           std::cout << "Size: " << cache_size << "/" << _max_size << std::endl;
           ARC_cache cache(cache_size);
           (this->*workload_map[_workload])(cache, _tracefile);
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
        parse_tracefile(filename, "b5b4908459d349a16a0416b1c5d6e79e5b3324491c9e08999a6f2630e1abfebb");
    }
    play_tracefile(cache, num_trace_played);

    mrc_stats[cache.capacity()] = cache.get_miss_rate();
    std::cout << "Miss Rate: " << cache.get_miss_rate() << "\n";
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
    uint32_t unique_file_num = 0;
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
    			total_size = rs + file_offset;
                trace_requests.push_back(std::make_tuple(filename, file_offset, total_size));
            }
            if (!file_map.contains(filename)) {
                file_map[filename] = unique_file_num;
                unique_file_num++;
            }
            if (total_size > max_file_size) {
                max_file_size = ((total_size/block_size) + 1)*block_size;
		}
	}
    create_trace_requests();
    std::ofstream ofs("requests.txt");
    for(auto i : trace_requests_blkid)
        ofs << i << '\n';
}

void
MRC::play_tracefile(ARC_cache &cache, uint32_t num_access) {
    uint32_t cur_acc_counter = 0;
	for (auto it = trace_requests.cbegin(); it != trace_requests.cend(); ++it) {
        std::string filename = std::get<0>(*it);
        uint32_t file_base_addr = file_map[filename] * max_file_size;
		uint32_t first_block_id = (std::get<1>(*it)+file_base_addr)/block_size;
		uint32_t last_block_id = (std::get<2>(*it)+file_base_addr)/block_size;
		for (uint32_t i = first_block_id; i <= last_block_id; i++){
			cache.access(i); // % cache.capacity()); 
			//std::cout << '\t' << i << std::endl;
            cur_acc_counter++;
            if(num_access != 0 && cur_acc_counter >= num_access)
                break;
		}
        if(num_access != 0 && cur_acc_counter >= num_access)
            break;
	}
}

void 
MRC::create_trace_requests(){
    for (auto it = trace_requests.cbegin(); it != trace_requests.cend(); ++it) {
        std::string filename = std::get<0>(*it);
        uint32_t file_base_addr = file_map[filename] * max_file_size;
		uint32_t first_block_id = (std::get<1>(*it)+file_base_addr)/block_size;
		uint32_t last_block_id = (std::get<2>(*it)+file_base_addr)/block_size;
		for (uint32_t i = first_block_id; i <= last_block_id; i++){
			this->trace_requests_blkid.push_back(i);
		}
	}
    std::cerr << "Total requests: " << this->trace_requests_blkid.size() << std::endl;
}


int main() {
    uint32_t min_size = 32*1024;
    uint32_t max_size = 4096*256*1024;
    method_e method = method_e::SLOPE;
    workload_e workload = workload_e::TRACE;
    uint32_t sampling_rate = 1;

    MRC mrc(method, workload, sampling_rate, min_size, max_size);

//    mrc.construct_mrc();
//    mrc.save_mrc("baseline.csv");
//
//    mrc.set_method(method_e::SLOPE);
//    mrc.construct_mrc();
//    mrc.save_mrc("slope.csv");
    mrc.set_num_trace_played(0);
//    mrc.set_tracefile("../thesios_traces/thesios-subset.csv");
    mrc.set_tracefile("../thesios_traces/data-00000-of-00100");
//    mrc.set_tracefile("../../data-00100");
    mrc.construct_mrc();
    mrc.save_mrc("trace-slope.csv");

    std::cout << "Running Baseline\n";
    mrc.set_method(method_e::BASELINE);
    mrc.construct_mrc();
    mrc.save_mrc("trace-baseline.csv");

    return 0;
}
