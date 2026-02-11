#include "tlb.h"
#include "stats.h"
#include "memory_manager.h"
#include <bits/stdc++.h>
#include "cache/cache_set_lru.h"

namespace ParametricDramDirectoryMSI
{
std::deque<IntPtr> pfq;

   std::map<IntPtr, std::map<IntPtr, uint64_t>> TLB::phist;
   std::deque<IntPtr> TLB::shadow_table;

   std::map<IntPtr, uint64_t> TLB::llt_hits;
   std::map<IntPtr, IntPtr> TLB::pc_hist;

   IntPtr TLB::last_pc = 0;

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

   // TODO: The repo_dir should be set automatically
   const char* env_path = std::getenv("PREDICTOR_CONFIG");
   std::string config_file;
   if (env_path != nullptr) {
       config_file = std::string(env_path);
   } else {
       std::string repo_dir = "/home/otegby/repos/code/";
       std::string benchmark_dir = "dpPred-cbPred/benchmarks/";
       std::string config_name = "predictor_config.txt";
       config_file = repo_dir + benchmark_dir + config_name;
   }

   load_settings(config_file);
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
   IntPtr temp = address & hw_page_bitmask;
   bool hit = m_cache.accessSingleLine(address, Cache::LOAD, NULL, 0, now, true);
   m_access++;

   if (isIfetch)
       last_pc = address;

   if (hit) {
       if (get_size() == llt_size) {
           llt_hits[temp]++;
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
TLB::findHash(IntPtr index, uint64_t bits) {
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
TLB::add_recent_pfn(IntPtr address) {
	address >>= sw_page_bitshift;
	if (pfq.size() < pfq_size) {
		pfq.push_back(address);
	} else {
		pfq.pop_front();
		pfq.push_back(address);
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
TLB::flushing_vpn_column(IntPtr temp_hash_vpn)
{
    uint64_t max_hash = 1ULL << pc_bits; 
    for (uint64_t pc_hash = 0; pc_hash < max_hash; pc_hash++) {
        phist[temp_hash_vpn][pc_hash] = 0;
    }
}

void
TLB::updating_phist(IntPtr evict_addr)
{
    IntPtr evict_vpn = evict_addr & hw_page_bitmask; 
    IntPtr evict_pc  = pc_hist[evict_vpn];

    IntPtr ev_vpn_hash = findHash(evict_vpn, vpn_bits);
    IntPtr ev_pc_hash  = findHash(evict_pc, pc_bits);

    if (llt_hits[evict_vpn] == 0) {
        phist[ev_vpn_hash][ev_pc_hash]++;
    } else {
        phist[ev_vpn_hash][ev_pc_hash] = 0;
    }
}

void
TLB::allocate(IntPtr address, SubsecondTime now)
{
   bool in_llt = get_size() == llt_size;
   if (dppred) {
      IntPtr temp_vpn = address & hw_page_bitmask;
      IntPtr temp_hash_vpn = findHash(temp_vpn, vpn_bits);
      IntPtr temp_hash_pc  = findHash(last_pc, pc_bits);

      if (in_llt) {
         pc_hist[temp_vpn] = last_pc;

         if (shadow_table_search(temp_vpn)) {
           flushing_vpn_column(temp_hash_vpn);
         }

         bool sat_thd = phist[temp_hash_vpn][temp_hash_pc] > phist_thd; 
         if (sat_thd) {
             ++m_bypass;
             shadow_table_insert(temp_vpn);
             add_recent_pfn(temp_vpn);
             return;
         } else { 
             llt_hits[temp_vpn] = 0;
         }
     }
  } 

   bool eviction;
   IntPtr evict_addr; 
   CacheBlockInfo evict_block_info;

   ++m_alloc;
   m_cache.insertSingleLine(address, NULL, &eviction, &evict_addr, &evict_block_info, NULL, now, NULL, false);
   if (dppred && eviction && in_llt) {
      updating_phist(evict_addr);
   }
}

template<typename T>
bool
TLB::read_config_value(const std::string& filename, const std::string& key, T& value) {
    std::ifstream file(filename.c_str());
    if (!file.is_open()) return false;
    
    std::string line;
    while (std::getline(file, line)) {
        size_t pos = line.find('=');
        if (pos != std::string::npos) {
            std::string k = line.substr(0, pos);
            std::string v = line.substr(pos + 1);
            if (k == key) {
                if (std::is_same<T, bool>::value) {
                    value = (std::stoi(v) != 0);
                } else if (std::is_same<T, uint64_t>::value) {
                    value = std::stoull(v);
                }
                return true;
            }
        }
    }
    return false;
}

void
TLB::load_settings(const std::string& config_file) {
    static bool printed = false;

    read_config_value(config_file, "DPPRED", dppred);
    read_config_value(config_file, "PHIST_THD", phist_thd);
    read_config_value(config_file, "PFQ_SIZE", pfq_size);
    read_config_value(config_file, "SHADOW_TABLE_SIZE", shadow_table_size);

    if (!printed) {
        std::cout << "=== LLT Settings ===" << std::endl;
        std::cout << "DPPRED: " << dppred << std::endl;
        std::cout << "PHIST_THD: " << phist_thd << std::endl;
        std::cout << "PFQ_SIZE: " << pfq_size << std::endl;
        std::cout << "SHADOW_TABLE_SIZE: " << shadow_table_size << std::endl;
        std::cout << "====================" << std::endl;
        printed = true;
  }
}

}

