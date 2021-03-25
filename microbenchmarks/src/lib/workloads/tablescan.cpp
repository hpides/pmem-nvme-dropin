#include <cstddef>

#include "workload.hpp"
#include "buffer_manager.hpp"

TableScanWorkload::TableScanWorkload(BufferManager& bm, std::size_t total_workload) :
    bm(bm), total_workload(total_workload) {}

void TableScanWorkload::run() {
    uint64_t processed_data = 0;
    bmlog::info("running tablescan.");
    AlignedMemoryBlock buf(bm.get_mem_alignment(), bm.get_page_size());
    uint64_t current_page_id = 0;
    while (processed_data < total_workload) {
        if (bm.pagein(*buf, current_page_id) == BM_READ_FAILURE) {
            bmlog::error("Paging in failed, aborting workload!");
            return;
        }
        
        processed_data += bm.get_page_size();
        current_page_id++;
        current_page_id %= bm.get_total_num_of_pages();
    }
}