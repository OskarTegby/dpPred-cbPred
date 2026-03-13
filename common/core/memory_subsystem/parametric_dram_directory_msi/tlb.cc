#include "tlb.h"
#include "stats.h"
#include "memory_manager.h"
#include <bits/stdc++.h>
#include "cache/cache_set_lru.h"

namespace ParametricDramDirectoryMSI
{

std::deque<IntPtr> recentPFN;

IntPtr lastPC;

std::deque<IntPtr> shadow_table;
uint64_t shadow_table_size = 2;
uint64_t pfq_size = 8;

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

UInt32 TLB::give_size()
{
   return m_size;
}

void TLB::setL3Controller(CacheCntlr *last_level)
{
   m_last_level = last_level;
}

void 
TLB::setDeadBit (IntPtr address){
  
   m_cache.setPrDeadBit (address);
}

bool
TLB::lookup(IntPtr address, SubsecondTime now, bool isIfetch, MemoryManager* mptr, bool allocate_on_miss)
{
   IntPtr temp = address & hw_page_bitmask;
   bool hit = m_cache.accessSingleLine(address, Cache::LOAD, NULL, 0, now, true);
   m_access++;

   if (isIfetch)
       lastPC = address;

   if (hit) {
       if (give_size() == llt_size) {
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
TLB::findHash (IntPtr ev_vpn, uint64_t bits)
{
   IntPtr last_part = ev_vpn;
   IntPtr lph = 0;
   int max_iter = 32 / bits;
   for (int i = 0; i < max_iter; ++i) {
        lph ^= (last_part % (1 << bits));
        last_part >>= bits;
   }
   return lph;
}

void
addRecentPFN(IntPtr addr) {
	addr >>= 17;
	if (recentPFN.size() < pfq_size) {
		recentPFN.push_back(addr);
	} else {
		recentPFN.pop_front();
		recentPFN.push_back(addr);
	}
}

bool
TLB::shadow_table_search (IntPtr vpn)
{
    bool res = false;
    for (uint64_t i = 0; i < shadow_table.size(); i++)
    {
        if (shadow_table[i] == vpn)
        {
            return true;
        }
    }
    return res;
}

void
TLB::shadow_table_insert (IntPtr vpn)
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
   if (give_size() == llt_size)
        insert_pc[(address & hw_page_bitmask)] = lastPC;

   IntPtr temp_vpn = address & hw_page_bitmask;
   IntPtr temp_hash_vpn = findHash ((address & hw_page_bitmask), vpn_bits);
   IntPtr temp_hash_pc =  findHash (lastPC, pc_bits);
   ++m_alloc;

   if (give_size() == llt_size)
   {
       bool res = shadow_table_search (temp_vpn);
       if (res == true)
       {
           for (int i = 0;i < (1 << pc_bits);i++)
           {
               hitCounter[temp_hash_vpn][i]= 0;
           }
       }
   }

   if (give_size() == llt_size && hitCounter[temp_hash_vpn][temp_hash_pc] > phist_thd) {
        ++m_bypass;
        shadow_table_insert (temp_vpn);
	addRecentPFN(address & hw_page_bitmask);
        return;
   } else if (give_size() == llt_size) {
        curHit[(address & hw_page_bitmask)] = 0;
   }
   bool eviction;
   IntPtr evict_addr;
   CacheBlockInfo evict_block_info;
   m_cache.insertSingleLine(address, NULL, &eviction, &evict_addr, &evict_block_info, NULL, now, NULL, false);
   if (eviction && give_size() == llt_size) {

        IntPtr ev_vpn_hash = findHash ((evict_addr & hw_page_bitmask), vpn_bits);
        IntPtr ev_pc_hash = findHash ((insert_pc[(evict_addr & hw_page_bitmask)]), pc_bits);
        if (!curHit[(evict_addr & hw_page_bitmask)]){
                hitCounter[ev_vpn_hash][ev_pc_hash]++;
        } else {
                hitCounter[ev_vpn_hash][ev_pc_hash] = 0;
        }
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

