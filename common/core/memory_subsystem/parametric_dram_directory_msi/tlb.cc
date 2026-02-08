#include "tlb.h"
#include "stats.h"
#include "memory_manager.h"
#include <bits/stdc++.h>
#include "cache/cache_set_lru.h"

namespace ParametricDramDirectoryMSI
{

std::deque<IntPtr> recentPFN;
uint64_t pfq_size = 8;

IntPtr lastPC;

std::deque<IntPtr> shadow_table;
uint64_t shadow_table_size = 2;

uint64_t llt_size = 1024;
uint64_t bypass_thd = 6;

uint64_t pc_bits = 6;
uint64_t vpn_bits = 4;
uint64_t index_size = 32;
uint64_t page_bitmask = 0xfffffffffffff000; 

std::map<IntPtr, IntPtr> insert_pc;

TLB::TLB(String name, String cfgname, core_id_t core_id, UInt32 num_entries, UInt32 associativity, TLB *next_level, UInt32 conf_count)
   : m_size(num_entries)
   , m_associativity(associativity)
   , m_cache(name + "_cache", cfgname, core_id, num_entries / associativity, associativity, SIM_PAGE_SIZE, "lru", CacheBase::PR_L1_CACHE)
   , m_next_level(next_level)
   , m_access(0)
   , m_miss(0)
   , m_alloc(0)
   , m_bypass(0)
   , m_conf_counter(conf_count)
{
   LOG_ASSERT_ERROR((num_entries / associativity) * associativity == num_entries, "Invalid TLB configuration: num_entries(%d) must be a multiple of the associativity(%d)", num_entries, associativity);

   registerStatsMetric(name, core_id, "access", &m_access);
   registerStatsMetric(name, core_id, "miss", &m_miss);
   registerStatsMetric(name, core_id, "bypass", &m_bypass);
   registerStatsMetric(name, core_id, "allocs", &m_alloc);
}

UInt32 TLB::get_size()
{
   return m_size;
}

void TLB::setL3Controller(CacheCntlr *last_level)
{
   m_last_level = last_level;
}

void 
TLB::setDeadBit(IntPtr address){
  
   m_cache.setPrDeadBit(address);
}

bool
TLB::lookup(IntPtr address, SubsecondTime now, bool isIfetch, MemoryManager* mptr, bool allocate_on_miss)
{
   IntPtr temp = address & page_bitmask;
   bool hit = m_cache.accessSingleLine(address, Cache::LOAD, NULL, 0, now, true);
   m_access++;

   if (isIfetch)
       lastPC = address;

   if (hit) {
       if (get_size() == llt_size) {
           curHit[temp]++;
       }
       return true;
   }
   m_miss++;

   SubsecondTime temp1;

   // L1 TLB cycles.
   temp1.setTime(3007512);   

   if (m_next_level)
   { 
      mptr->incrElapsedTime(temp1, ShmemPerfModel::_USER_THREAD);
      hit = m_next_level->lookup(address, now, false, mptr, true /* no allocation */);
   }
   if (allocate_on_miss)
   {
         allocate(address, now);
   }
   return hit;
}

IntPtr
TLB::findHash(IntPtr index, uint64_t bits)
{
   IntPtr remaining = index;
   IntPtr hash = 0;
   int max_iter = index_size / bits;
   for (int i = 0; i < max_iter; ++i) {
        hash ^= (remaining % (1 << bits));
        remaining >>= bits;
   }
   return hash;
}

void
addRecentPFN(IntPtr address) {
	address >>= 17;
	if (recentPFN.size() < pfq_size) {
		recentPFN.push_back(address);
	} else {
		recentPFN.pop_front();
		recentPFN.push_back(address);
	}
}

bool
TLB::shadow_table_search(IntPtr vpn)
{
    for (uint64_t i = 0; i < shadow_table.size(); i++)
    {
        if (shadow_table[i] == vpn)
        {
            return true;
        }
    }
    return false;
}

void
TLB::shadow_table_insert(IntPtr vpn)
{
    if (shadow_table.size() == shadow_table_size)
    {
        shadow_table.pop_front();
    }
    shadow_table.push_back(vpn);
}

void
TLB::allocate(IntPtr address, SubsecondTime now)
{
   bool in_llt = get_size() == llt_size;

   IntPtr temp_vpn = address & page_bitmask;
   IntPtr temp_hash_vpn = findHash(temp_vpn, vpn_bits);
   IntPtr temp_hash_pc  = findHash(lastPC, pc_bits);
   ++m_alloc;

   if (in_llt) {
      insert_pc[temp_vpn] = lastPC;

      if (shadow_table_search(temp_vpn))
      {
          for (uint64_t i = 0; i < 64; i++)
          {
              hitCounter[temp_hash_vpn][i] = 0;
          }
      }

      bool sat_thd = hitCounter[temp_hash_vpn][temp_hash_pc] > bypass_thd; 
      if (sat_thd) {
          ++m_bypass;
          shadow_table_insert(temp_vpn);
          addRecentPFN(temp_vpn);
          return;
      } else { 
          curHit[temp_vpn] = 0;
      }
  }

   bool eviction;
   IntPtr evict_addr; 
   CacheBlockInfo evict_block_info;

   m_cache.insertSingleLine(address, NULL, &eviction, &evict_addr, &evict_block_info, NULL, now, NULL, false);
   if (eviction && in_llt) {
        IntPtr evict_vpn = evict_addr & page_bitmask; 
        IntPtr evict_pc  = insert_pc[evict_vpn];

        IntPtr ev_vpn_hash = findHash(evict_vpn, vpn_bits);
        IntPtr ev_pc_hash  = findHash(evict_pc, pc_bits);

        if (curHit[evict_vpn] == 0) {
            hitCounter[ev_vpn_hash][ev_pc_hash]++;
        } else {
            hitCounter[ev_vpn_hash][ev_pc_hash] = 0;
        }
   }
}

}

