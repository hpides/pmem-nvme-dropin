#include <cstddef>
#include <cstring>
#include <random>
#include <vector>

#include "buffer_manager.hpp"
#include "workload.hpp"
#include "util.hpp"


BufferManagementWorkload::BufferManagementWorkload(BufferManager& bm, std::size_t total_workload, float write_proportion, int random_pages, uint32_t pattern_seed, uint64_t target_pages)
    : bm(bm), total_workload(total_workload), write_proportion(write_proportion), pattern_seed(pattern_seed), target_pages(target_pages), max_page_id(bm.get_total_num_of_pages() - 1) {
    for (int i = 0; i < random_pages; i++) {
        random_page_pool.emplace_back(bm.get_mem_alignment(), bm.get_page_size());
        for (unsigned int j = 0; j < bm.get_page_size(); j++) {
                ((char*)(*(random_page_pool[i])))[j] = static_cast<unsigned char>(next_random_data());
            }
        if (bm.get_page_size() >= 16)
            strcpy(((char*)(*(random_page_pool[i]))), "TheCakeIsALie");
    }
}

void BufferManagementWorkload::run() {
    uint64_t processed_data = 0;

    AlignedMemoryBlock buf(bm.get_mem_alignment(), target_pages * bm.get_page_size());
    uint64_t read_target_page = 0;
    while (processed_data < total_workload) {
        uint64_t page_id = next_page_id();

        if (do_write()) {
            // write something
            int random_poolpage_id = next_random_pagepool_id();
            int random_position = next_random_position(bm.get_page_size()-1);
            ((char*)(*(random_page_pool[random_poolpage_id])))[random_position] = static_cast<unsigned char>(next_random_data());
            bm.pageout(*(random_page_pool[random_poolpage_id]), page_id);
        } else {
            // only a read
            char *tgt = (char*)*buf + bm.get_page_size() * read_target_page++;
            read_target_page %= target_pages;
            if (bm.pagein(tgt, page_id) == BM_READ_FAILURE) {
                bmlog::error("Paging in failed, aborting workload!");
                return;
            }
        }
        processed_data += bm.get_page_size();
        //bmlog::info("processed data: " + std::to_string(processed_data));
    }
}

std::mt19937& BufferManagementWorkload::gen() {
    static std::mt19937 generator = CustomSeededEngine(pattern_seed);
    return generator;
}

bool BufferManagementWorkload::do_write() {
    static std::uniform_real_distribution<> dis(0.0, 1.0);
    double val = dis(gen());
    return val < write_proportion;
}

uint64_t BufferManagementWorkload::next_page_id() {
    static std::uniform_int_distribution<uint64_t> dis(0, max_page_id);
    return dis(gen());
}

int BufferManagementWorkload::next_random_pagepool_id() {
    static std::uniform_int_distribution<uint64_t> dis(0, random_page_pool.size() - 1);
    return dis(gen());
}

int BufferManagementWorkload::next_random_data() {
    static std::uniform_int_distribution<int> dis(0, 255);
    return dis(gen());
}

int BufferManagementWorkload::next_random_position(int maxval) {
    std::uniform_int_distribution<int> dis(0, maxval);
    return dis(gen());
}
