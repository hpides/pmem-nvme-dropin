#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "iowrapper.hpp"

enum BMStatus {
    BM_READ_FAILURE,
    BM_READ_SUCCESS,
    BM_WRITE_FAILURE,
    BM_WRITE_SUCCESS
};

bool ok(const BMStatus status);

class BufferManager {
    public:
        BufferManager() = delete;
        explicit BufferManager(std::vector<std::string>& dirs, const char* file_suffix, const std::size_t page_size, const std::size_t pages_per_buffer_file, const bool use_fadvise, const bool pmem_use_cacheline_granularity,  const bool mmap_use_map_sync, std::function<IOWrapper*(struct IOWrapperConfig&)> create_io_wrapper, const bool fadv_random, const bool fadv_sequential, const bool madv_random, const bool madv_sequential, const bool mmap_populate);
        ~BufferManager();
        BMStatus pagein(void *dest, const uint64_t page_id);
        BMStatus pageout(void *src, const uint64_t page_id);
        uint64_t get_total_num_of_pages() const;
        uint32_t get_mem_alignment();
        std::size_t get_page_size() const;
    private:
        int lookup(IOWrapper **responsible_wrapper, uint64_t *internal_id, const uint64_t page_id);
        std::vector<IOWrapper*> buffers;
        std::size_t page_size;
        std::size_t pages_per_buffer_file;
};
