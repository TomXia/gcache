#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <vector>
#include <list>
#include <unordered_map>

#include "node.h"
#include "table.h"

namespace gcache {
class ARC_cache{
public:
    ARC_cache(int capacity) : capacity_(capacity), p(0), stat_hit(0), stat_miss(0) {}
    ARC_cache(const ARC_cache&) = delete;
    ARC_cache(ARC_cache&&) = delete;
    ARC_cache& operator=(const ARC_cache&) = delete;
    ARC_cache& operator=(ARC_cache&&) = delete;

    size_t capacity() const { return capacity_; }

    uint32_t get_hit() const { return stat_hit; }
    uint32_t get_miss() const { return stat_miss; }
    void rst_stat() { stat_miss = stat_hit = 0; }

    void access(uint32_t key)
    {
        auto t1_iter = std::find(T1.begin(), T1.end(), key);
        auto t2_iter = std::find(T2.begin(), T2.end(), key);
        if (t1_iter != T1.end() || t2_iter != T2.end()) 
        {
            // case 1: Cache hit, move to T2
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
                if(T1.size() < capacity_)
                {
                    //A remove LRU in B1
                    u_int32_t key = B1.back();
                    in_ghost[key] = false;
                    B1.pop_back();
                    replace(false);
                }
                else //A delete LRU in T1
                    T1.pop_back();
            }
            else
            {
                if(T1.size() + T2.size() + B1.size() + B2.size() >= capacity_)
                {
                    if(T1.size() + T2.size() + B1.size() + B2.size() == 2*capacity_)
                    {
                        //delete LRU in B2
                        u_int32_t key = B2.back();
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
    uint32_t stat_miss, stat_hit;

    std::list<uint32_t> T1, T2;
    std::list<uint32_t> B1, B2;
    std::unordered_map<uint32_t, bool> in_ghost;

    void replace(bool in_b2)
    {
        if (!T1.empty() && (T1.size() > p || (in_b2 && T1.size() == p)))//arbitrary
        {
            // Evict from T1 to B1
            u_int32_t key = T1.back();
            T1.pop_back();
            B1.push_front(key);
            in_ghost[key] = true; // Track in ghost list
        } else {
            // Evict from T2 to B2
            int key = T2.back();
            T2.pop_back();
            B2.push_front(key);
            in_ghost[key] = true; // Track in ghost list
        }
    }
};

}  // namespace gcache
