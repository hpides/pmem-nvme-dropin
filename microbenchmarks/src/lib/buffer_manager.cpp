#include <algorithm>
#include <cstdint>
#include <vector>
#include <numeric>

#include "buffer_manager.hpp"
#include "termcolor/termcolor.h"
#include "util.hpp"

bool ok(const BMStatus status) {
    switch (status) {
    case BM_READ_FAILURE:
    case BM_WRITE_FAILURE:
        return false;
    default:
        break;
    }
    return true;
}

BufferManager::BufferManager(std::vector<std::string>& dirs, const char *file_suffix, const std::size_t page_size, const std::size_t pages_per_buffer_file, const bool use_fadvise, const bool pmem_use_cacheline_granularity,  const bool mmap_use_map_sync,  std::function<IOWrapper*(struct IOWrapperConfig&)> create_io_wrapper, const bool fadv_random, const bool fadv_sequential, const bool madv_random, const bool madv_sequential, const bool mmap_populate)
 : page_size(page_size), pages_per_buffer_file(pages_per_buffer_file) {
    std::for_each(dirs.begin(), dirs.end(), [&](std::string& dir) {
        struct IOWrapperConfig config{dir.c_str(), file_suffix, use_fadvise, pmem_use_cacheline_granularity, mmap_use_map_sync, 0, fadv_random, fadv_sequential, madv_random, madv_sequential, mmap_populate};
        auto newBuf = create_io_wrapper(config);
        if (pages_per_buffer_file * page_size > newBuf->get_filesize())  {
            crash("VERY SAD FAKE NEWS: buffer in directory "  + dir + " is too small :C");
        }

        buffers.push_back(newBuf);
    });
}

BufferManager::~BufferManager() {
    for (IOWrapper *w : buffers) delete w;
}

BMStatus BufferManager::pagein(void *dest, const uint64_t page_id) {
    IOWrapper *responsibleWrapper;
    uint64_t internal_page_id;
    if (lookup(&responsibleWrapper, &internal_page_id, page_id) != 0) {
        bmlog::error("tried to read from invalid page id");
        return BM_READ_FAILURE;
    }
    std::size_t position = internal_page_id * page_size;
    if (responsibleWrapper->read(dest, position, page_size) != 0) {
        return BM_READ_FAILURE;
    }
    return BM_READ_SUCCESS;
}

int BufferManager::lookup(IOWrapper **responsible_wrapper, uint64_t *internal_id, const uint64_t page_id) {
    if (page_id >= buffers.size() * pages_per_buffer_file) return -1;
    std::size_t bufferId = page_id % buffers.size();
    *responsible_wrapper = buffers[bufferId];
    *internal_id = page_id / buffers.size();
    return 0;
}

BMStatus BufferManager::pageout(void *src, const uint64_t page_id) {
    IOWrapper *responsibleWrapper;
    uint64_t internal_page_id;
    if (lookup(&responsibleWrapper, &internal_page_id, page_id) != 0) {
        bmlog::error("tried to write to invalid page id");
        return BM_WRITE_FAILURE;
    }
    std::size_t position = internal_page_id * page_size;
    if (responsibleWrapper->write(src, position, page_size) != 0) {
        return BM_WRITE_FAILURE;
    }
    return BM_WRITE_FAILURE;
}

uint64_t BufferManager::get_total_num_of_pages() const {
    return static_cast
    <uint64_t>(pages_per_buffer_file * buffers.size());
}

uint32_t BufferManager::get_mem_alignment() {
    uint32_t m = 1;
    for (auto io : buffers) {
        m = std::lcm(m, io->get_alignment());
    }
    // std::accumulate(buffers.begin(), buffers.end(), 1, [&](uint32_t m, IOWrapper* io){ return std::lcm(m, io->get_alignment()); })
    return m;
}

std::size_t BufferManager::get_page_size() const {
    return page_size;
}
