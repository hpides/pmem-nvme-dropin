#include <cstddef>
#include <cstring>
#include <random>
#include <functional>

#include "workload.hpp"
#include "buffer_manager.hpp"
#include "util.hpp"

LoggingWorkload::LoggingWorkload(BufferManager& bm, std::size_t total_workload, std::size_t log_entry_size, uint32_t pattern_seed, bool committing) :
    bm(bm), total_workload(total_workload), log_entry_size(log_entry_size), pattern_seed(pattern_seed), committing(committing) {}

void LoggingWorkload::run() {
    uint64_t processed_data = 0;
    bmlog::info("running logging.");
    AlignedMemoryBlock buf(bm.get_mem_alignment(), bm.get_page_size());
    AlignedMemoryBlock logbuf(bm.get_mem_alignment(), log_entry_size);
    AlignedMemoryBlock headerbuf(bm.get_mem_alignment(), bm.get_page_size());
    uint64_t current_page_id = committing ? 1 : 0;
    std::function<void()> update_watermark = [&](){
        static uint64_t entries = 0;
        *reinterpret_cast<uint64_t*>(*logbuf) = ++entries;
        if (bm.pageout(*headerbuf, 0) == BM_WRITE_FAILURE) {
            crash("Paging out (@commit) failed, aborting workload");
        }
    };
    while (processed_data + log_entry_size <= total_workload) {
        // alter data
        static_cast<char*>(*logbuf)[next_random_data(log_entry_size)] = next_random_data(255);

        // write log entry
        std::size_t to_write = log_entry_size;
        while (to_write > 0) {
            std::size_t pos_in_logbuf = log_entry_size - to_write;
            std::size_t pos_in_buf = (processed_data + pos_in_logbuf) % bm.get_page_size();
            if (bm.get_page_size() - pos_in_buf <= to_write) { // write until end of buffer
                std::memcpy(static_cast<char*>(*buf) + pos_in_buf, static_cast<char*>(*logbuf) + pos_in_logbuf, bm.get_page_size() - pos_in_buf);
                if (bm.pageout(*buf, current_page_id++) == BM_WRITE_FAILURE) {
                    bmlog::error("Paging out failed, aborting workload!");
                    return;
                }
                to_write -= bm.get_page_size() - pos_in_buf;
            } else { // write rest of log
                std::memcpy(static_cast<char*>(*buf) + pos_in_buf, static_cast<char*>(*logbuf) + pos_in_logbuf, to_write);
                if (bm.pageout(*buf, current_page_id) == BM_WRITE_FAILURE) {
                    bmlog::error("Paging out failed, aborting workload!");
                    return;
                }
                to_write -= to_write;
            }
        }
        if (committing) update_watermark();

        processed_data += log_entry_size;
    }
}

std::mt19937& LoggingWorkload::gen() {
    static std::mt19937 generator = CustomSeededEngine(pattern_seed);
    return generator;
}

int LoggingWorkload::next_random_data(int max) {
    std::uniform_int_distribution<int> dis(0, max);
    return dis(gen());
}
