
#ifdef __linux

#include <libpmem2.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>

#include "util.hpp"
#include "iowrapper.hpp"

LibPMIOWrapper::LibPMIOWrapper(struct IOWrapperConfig& config) {

    bufferFilename = std::string(config.directory) + BUFFER_FILE_BASENAME + std::string(config.file_suffix);
    fd = open(bufferFilename.c_str(), open_flags(), 0666);
    if (fd == -1) {
        perror("open");       
        crash(std::string("could not open buffer file ") + bufferFilename);
    }
#ifdef __linux
    if (config.use_fadvise) {
        if (posix_fadvise(fd, 0, 0, POSIX_FADV_DONTNEED) != 0) {
            perror("posix_fadvise");
            bmlog::warning("posix_fadvise did not succeed");
        }
    }

    if (config.fadv_sequential) {
        if (posix_fadvise(fd, 0, 0, POSIX_FADV_SEQUENTIAL) != 0) {
            perror("posix_fadvise");
            bmlog::warning("posix_fadvise did not succeed");
        }
    }

    if (config.fadv_random) {
        if (posix_fadvise(fd, 0, 0, POSIX_FADV_RANDOM) != 0) {
            perror("posix_fadvise");
            bmlog::warning("posix_fadvise did not succeed");
        }
    }
#endif

    if (pmem2_config_new(&pmcfg)) {
        pmem2_perror("pmem2_config_new");
        crash(std::string("could not create pmem2_config for ") + bufferFilename);
    }

    if (pmem2_source_from_fd(&pmsrc, fd)) {
        pmem2_perror("pmem2_source_from_fd");
        crash(std::string("could not create pmem2_source for ") + bufferFilename);
    }

    auto granularity = PMEM2_GRANULARITY_PAGE;
    if (config.pmem_use_cacheline_granularity) granularity = PMEM2_GRANULARITY_CACHE_LINE;
    
    if (pmem2_config_set_required_store_granularity(pmcfg, granularity)) {
        pmem2_perror("pmem2_config_set_required_store_granularity");
        crash(std::string("could not set store_granularity for ") + bufferFilename);
    }

    if (pmem2_map_new(&pmmap, pmcfg, pmsrc)) {
        pmem2_perror("pmem2_map_new");
        crash(std::string("could not create pmem2_mapping for ") + bufferFilename);
    }

    map_addr = pmem2_map_get_address(pmmap);
    pmpersist_fn = pmem2_get_persist_fn(pmmap);
    pmmemcpy_fn = pmem2_get_memcpy_fn(pmmap);
}

LibPMIOWrapper::~LibPMIOWrapper() {
    if (pmem2_map_delete(&pmmap) != 0) {
        pmem2_perror("pmem2_map_delete");
        crash(std::string("could not delete pmem2_map of file") + bufferFilename);
    }
    if (pmem2_source_delete(&pmsrc) != 0) {
        pmem2_perror("pmem2_source_delete");
        crash(std::string("could not delete pmem2_source of file"));
    }
    if (pmem2_config_delete(&pmcfg) != 0) {
        pmem2_perror("pmem2_config_delete");
        crash(std::string("could not delete pmem2_config of file"));
    }

    if (close(fd) != 0) {
        perror("close");
        crash(std::string("could not close buffer file ") + bufferFilename);
    }
}

int LibPMIOWrapper::read(void *dest, std::size_t position, std::size_t len) {
    std::memcpy(dest, ((char*)map_addr) + position, len);
    return 0;
}

int LibPMIOWrapper::write(void *src, std::size_t position, std::size_t len) {
    pmmemcpy_fn(((char*)map_addr) + position, src, len, 0);
    return 0;
}

const char* LibPMIOWrapper::get_filename() const {
    return bufferFilename.c_str();
}

int LibPMIOWrapper::open_flags() {
    return O_RDWR | O_CREAT;
}

#endif

