#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <vector>
#include <list>
#include <unordered_map>
#include <map>
#include <cmath>

#include "node.h"
#include "table.h"

#define strong_assert(condition)                                          \
    do {                                                                    \
        if (!(condition)) {                                                 \
            fprintf(stderr, "[ASSERT FAILED] %s:%d: %s()\n",            \
                    __FILE__, __LINE__, __func__);                 \
            abort();                         \
        }                                                                   \
    } while (0)

#ifdef DEBUG
#define DOUT std::cerr
#else
#define DOUT 0 && std::cerr
#endif

namespace gcache
{
class ARC_cache
{
public:
    ARC_cache(uint32_t capacity) : capacity_(capacity), p(0), stat_miss(0), stat_hit(0), stat_ev_t1(0), stat_ev_t2(0), stat_ev(0) {}
    ARC_cache(const ARC_cache&) = delete;
    ARC_cache(ARC_cache&&) = delete;
    ARC_cache& operator=(const ARC_cache&) = delete;
    ARC_cache& operator=(ARC_cache&&) = delete;

    size_t capacity() const { return capacity_; }
    size_t actual_size() const { return T1.size() + T2.size(); }
    size_t all_size() const { return T1.size() + T2.size() + B1.size() + B2.size(); }

    uint32_t get_hit()   const { return stat_hit; }
    uint32_t get_miss()  const { return stat_miss; }
    uint32_t get_ev()  const { return stat_ev; }
    float get_hit_rate() const { return (stat_miss + stat_hit == 0 ? NAN : (float)stat_hit / (stat_miss + stat_hit)); }
    float get_miss_rate()const { return (stat_miss + stat_hit == 0 ? NAN : (float)stat_miss / (stat_miss + stat_hit)); }
    void rst_stat() { stat_miss = stat_hit = stat_ev_t1 = stat_ev_t2 = stat_ev = 0; }

    void print_sizes()
    {
        std::cout << "T1: " << T1.size() << '\t'
                  << "B1: " << B1.size() << '\t'
                  << "T2: " << T2.size() << '\t'
                  << "B2: " << B2.size() << std::endl;
    }

    void access(uint32_t key)
    {
        auto t1_iter = std::find(T1.begin(), T1.end(), key);
        auto t2_iter = std::find(T2.begin(), T2.end(), key);
        if (t1_iter != T1.end() || t2_iter != T2.end()) 
        {
            // case 1: Cache hit, move to T2
            DOUT << "case 1" << std::endl;
            stat_hit++;
            if (t1_iter != T1.end())
                T1.erase(t1_iter);
            else
                T2.erase(t2_iter);
            T2.push_front(key);
        } 
        else if (in_ghost[key])
        {
            auto b1_iter = std::find(B1.begin(), B1.end(), key);
            auto b2_iter = std::find(B2.begin(), B2.end(), key);
            stat_miss++;
            // case 2
            DOUT << "case 2" << std::endl;
            if(b1_iter != B1.end())
            {
                p = std::min(p + std::max(1, (int)B2.size() / (int)B1.size()), capacity_);
                replace(false);
                B1.erase(b1_iter);
                in_ghost[key] = false; // Remove from ghost list
            }
            // case 3
            else
            {
                DOUT << "case 3" << std::endl;
                p = std::max(p - std::max(1, (int)B1.size() / (int)B2.size()), 0u);
                replace(true);
                B2.erase(b2_iter);
                in_ghost[key] = false; // Remove from ghost list
            }
            T2.push_front(key);
        } 
        else
        {
            // case 4, cache miss, insert into T1
            stat_miss++;
            if(T1.size() + B1.size() == capacity_)
            {
                DOUT << "case 4A" << std::endl;
                if(T1.size() < capacity_)
                {
                    //A remove LRU in B1
                    uint32_t key = B1.back();
                    in_ghost[key] = false;
                    B1.pop_back();
                    replace(false);
                }
                else //A delete LRU in T1
                {
                    T1.pop_back();
                    stat_ev++;
                }
            }
            else
            {
                DOUT << "case 4B" << std::endl;
                if(T1.size() + T2.size() + B1.size() + B2.size() >= capacity_)
                {
                    if(T1.size() + T2.size() + B1.size() + B2.size() == 2*capacity_)
                    {
                        //delete LRU in B2
                        uint32_t key = B2.back();
                        B2.pop_back();
                        in_ghost[key] = false; // Track in ghost list
                    }
                    replace(false);
                }
            }
            T1.push_front(key);
        }
    }
private:
    uint32_t capacity_;
    uint32_t p;
    uint32_t stat_miss, stat_hit, stat_ev_t1, stat_ev_t2, stat_ev;

    std::list<uint32_t> T1, T2;
    std::list<uint32_t> B1, B2;
    std::unordered_map<uint32_t, bool> in_ghost;

    void replace(bool in_b2)
    {
        if (!T1.empty() && (T1.size() > p || (in_b2 && T1.size() == p)))
        {
            // Evict from T1 to B1
            uint32_t key = T1.back();
            T1.pop_back();
            B1.push_front(key);
            in_ghost[key] = true; // Track in ghost list
            stat_ev++;
        } else {
            // Evict from T2 to B2
            uint32_t key = T2.back();
            T2.pop_back();
            B2.push_front(key);
            in_ghost[key] = true; // Track in ghost list
            stat_ev++;
        }
    }
};

class ARC_mrc
{
public:
    ARC_mrc(const ARC_mrc&) = delete;
    ARC_mrc(ARC_mrc&&) = delete;
    ARC_mrc(uint32_t min_size, uint32_t max_size, uint32_t min_ticks, uint32_t max_ticks):
    min_size(min_size), max_size(max_size), min_ticks(min_ticks), max_ticks(max_ticks)
    {
        int init_ticks = (min_ticks + max_ticks) / 2;
        for(uint32_t i = min_size; i < max_size; i += init_ticks)
            cache_list.insert(std::make_pair(i, new ARC_cache(i)));
    }

    uint32_t get_num_points()
    {
        return cache_list.size();
    }

    void get_miss_rates(std::vector<std::pair<uint32_t, float> > &dst)
    {
        for(auto i : cache_list)
            dst.push_back(std::make_pair(i.first, i.second->get_miss_rate()));
    }

    void access(uint32_t key)
    {
        for(auto i : cache_list)
            i.second->access(key);
    }
    void reset_stat()
    {
        for(auto i : cache_list)
            i.second->rst_stat();
    }

private:
    std::map<uint32_t, ARC_cache*> cache_list;

    uint32_t min_size, max_size, min_ticks, max_ticks;

    void rm_point(uint32_t cache_size)
    {
        auto iter = cache_list.find(cache_size);
        strong_assert(iter != cache_list.end());
        delete iter->second;
        cache_list.erase(iter);
    }

    void add_point(uint32_t cache_size)
    {
        strong_assert( cache_list.find(cache_size) == cache_list.end());
        cache_list.insert(std::make_pair(cache_size, new ARC_cache(cache_size)));
    }
    
};

}  // namespace gcache
