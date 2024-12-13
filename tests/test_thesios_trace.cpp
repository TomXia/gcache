#include <algorithm>
#include <cstdlib>
#include <cstdio>
#include <fstream> 
#include <map> 
#include <sstream>
#include <string>
#include <tuple>

#include "gcache/ghost_cache.h"
#include "gcache/node.h"

#define BLK_SIZE (4096)
#define MIN      (128)
#define TICK     (256)
#define MAX      (1048576)

using namespace gcache;

// tuple should be in the order of min_offset, max_size
typedef std::tuple<uint32_t, uint32_t> trace_tuple;

std::map <std::string, trace_tuple> mapping; 

uint32_t max_size = 0;
uint32_t min_offset = -1;

GhostCache<> test_cache(TICK, MIN, MAX);

void parse_csv(std::string filename, std::string app_of_interest){
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
			std::cout << "Request IO, Offset or Filename column not found in data!" << std::endl;
			std::cout << "Request IO Size Bytes column (" << rs_col << ", index)" << std::endl;
			std::cout << "Offset Size column (" << offset_col << ", index)" << std::endl;
			std::cout << "Filename column (" << fname_col << ", index)" << std::endl;
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
				if (cell != app_of_interest && (!app_of_interest.empty())) {
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
			if (!mapping.count(filename)) {
				mapping.insert(std::make_pair(filename,
					       std::make_tuple(
						       0xFFFFFFFF,
						       0)));
			}
			total_size = rs + file_offset;
			if (file_offset < std::get<0>(mapping[filename])) {
				std::get<0>(mapping[filename]) = file_offset;
			}
			if (total_size > std::get<1>(mapping[filename])) {
				std::get<1>(mapping[filename]) = total_size;
				std::cout << "New max rq size is " << total_size << std::endl;
			}
		}
	
	}
}

uint32_t warm_cache() {
	uint32_t prev_boundary = 0; 
	for (auto it = mapping.cbegin(); it != mapping.cend(); ++it) {
		if (it->first.empty())
			continue;
		uint32_t curr_min_offset = std::get<0>(it->second);
		uint32_t curr_max_size = std::get<1>(it->second);
		uint32_t boundary = ((curr_max_size - curr_min_offset) / BLK_SIZE) + prev_boundary;
		std::cout << "Warming up all blks of: " << it->first << std::endl;
		for (uint32_t i = prev_boundary; i < boundary; i++){
			test_cache.access(i); 
			std::cout << '\t' << i << std::endl;
		}
		prev_boundary = boundary; 
	}
	test_cache.reset_stat();
	std::cout << "Max boundary: " << prev_boundary << std::endl;
	return prev_boundary; 
	
}

void print_trace_data(){
	for (auto it = mapping.cbegin(); it != mapping.cend(); ++it) {
		if (it->first.empty()) {
			continue;
		}
		std::cout << "Filename: " << it->first << std::endl;
		std::cout << "\t Minimum Offset: " << std::get<0>(it->second) << std::endl;
		std::cout << "\t Max Size (max request + offset): " << std::get<1>(it->second) << std::endl;


	}

}

void access_all_entries(uint32_t boundary) {
	for (uint32_t i = 0; i < boundary; i++){
		uint32_t entry = rand() % boundary;
		std::cout << "Accessing entry " << entry << std::endl;
		test_cache.access(entry);
		for (uint32_t s = test_cache.get_min_size(); s <= test_cache.get_max_size(); s+=TICK){ 
			std::cout << '\t' << s << ", " << test_cache.get_miss_rate(s) <<std::endl;
			
		}
		std::cout << std::endl;
	} 
	
}

int main(int argc, char *argv[]) {
	if (argc < 2) {
		std::cout << "Please enter a .csv or cluster data trace for thesios." << std::endl;
	} else {
		parse_csv(argv[1], "bigtable"); // Pass in "" if you want *all* entries to be considered
		print_trace_data();
		uint32_t boundary = warm_cache();
		std::cout << boundary << std::endl;
		access_all_entries(boundary);
	}


}
