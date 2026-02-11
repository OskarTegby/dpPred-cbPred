#ifndef TLB_H
#define TLB_H

#include "fixed_types.h"
#include "cache.h"
#include "cache_cntlr.h"
#include <bits/stdc++.h>

namespace ParametricDramDirectoryMSI
{
   class TLB
   {
      private:
         static const UInt32 SIM_PAGE_SHIFT = 12; // 4KB
         static const IntPtr SIM_PAGE_SIZE = (1L << SIM_PAGE_SHIFT);
         static const IntPtr SIM_PAGE_MASK = ~(SIM_PAGE_SIZE - 1);

         UInt32 m_size;
         UInt32 m_associativity;
         Cache m_cache;
         CacheCntlr *m_last_level;

         TLB *m_next_level;

         UInt64 m_access, m_miss, m_alloc, m_bypass;
         UInt32 m_conf_counter = 2;

         static std::map<IntPtr, std::map<IntPtr, uint64_t>> phist;
         static std::deque<IntPtr> shadow_table;

         static std::map<IntPtr, uint64_t> llt_hits;
         static std::map<IntPtr, IntPtr> pc_hist;

         static IntPtr last_pc;

         uint64_t llt_size = 1024;
         uint64_t phist_thd = 6;

         uint64_t pfq_size = 8;
         uint64_t shadow_table_size = 2;
        
         uint64_t pc_bits = 6;
         uint64_t vpn_bits = 4;
         uint64_t index_size = 32;

         uint64_t hw_page_bitmask = 0xfffffffffffff000;    // 4kB  pages
         uint64_t sw_page_bitshift = 17;                   // 12kB pages
      public:
         TLB(String name, String cfgname, core_id_t core_id, UInt32 num_entries, UInt32 associativity, TLB *next_level, UInt32 conf_count = 2);
         UInt32 get_size();
         void setL3Controller(CacheCntlr*);
         void setDeadBit (IntPtr address);
         bool lookup(IntPtr address, SubsecondTime now, bool isIfetch, MemoryManager *mptr, bool allocate_on_miss = true);
  
         IntPtr findHash(IntPtr ev_vpn, uint64_t bits);
         void add_recent_pfn(IntPtr address);
         bool shadow_table_search(IntPtr vpn);
         void shadow_table_insert(IntPtr vpn);
         void flushing_vpn_column(IntPtr temp_hash_vpn);
         void updating_phist(IntPtr evict_addr);

         void allocate(IntPtr address, SubsecondTime now);
   };
}

#endif // TLB_H
