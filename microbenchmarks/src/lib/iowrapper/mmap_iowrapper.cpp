#include <cstring>
#include <string>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

#ifdef __linux
#include <linux/version.h>
#endif

#include "util.hpp"
#include "iowrapper.hpp"

MmapIOWrapper::MmapIOWrapper(struct IOWrapperConfig& config) {
    bufferFilename = std::string(config.directory) + BUFFER_FILE_BASENAME + std::string(config.file_suffix);
    fd = open(bufferFilename.c_str(), open_flags(), 0666);
    mempagesize = getpagesize();
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
    map_length = get_filesize();
    
    auto map_flag = MAP_SHARED;
    if (config.use_map_sync)
#ifdef __linux
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,15,0)
#if __GLIBC__ >= 2
#if __GLIBC__ > 2	    
        map_flag = MAP_SHARED_VALIDATE | MAP_SYNC;
#else
#if __GLIBC_MINOR__ == 27
#define MAP_SHARED_VALIDATE 0x3
#define MAP_SYNC 0x80000
#endif
#if __GLIBC_MINOR__ >= 27
        map_flag = MAP_SHARED_VALIDATE | MAP_SYNC;
#else
        crash(std::string("linux kernel version is >= 4.15, but glibc is too old (< 2.27)! map_sync is unsupported."));
#endif	
#endif
#else
        crash(std::string("linux kernel version is >= 4.15, but glibc is too old (< 2)! map_sync is unsupported."));
#endif
#else
        crash(std::string("linux kernel version too old for map_sync flag!"));
#endif
#else
        bmlog::warning("MAP_SYNC not available on this system!");
#endif

#ifdef __linux
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,46)
    if (config.mmap_populate) map_flag |= MAP_POPULATE;
#endif
#endif

    map_addr = (char*)mmap(NULL, map_length, PROT_READ | PROT_WRITE, map_flag, fd, 0);
    if (map_addr == MAP_FAILED) {
        perror("mmap");
        crash("IOWrapper: mmap failed");
    }

#ifdef __linux
    if (config.madv_random) {
        if (posix_madvise(map_addr, map_length, POSIX_MADV_RANDOM) != 0) {
		perror("posix_madvise");
		bmlog::warning("posix_madvise did not succeed");
	}
    }
    if (config.madv_sequential) {
        if (posix_madvise(map_addr, map_length, POSIX_MADV_SEQUENTIAL) != 0) {
		perror("posix_madvise");
		bmlog::warning("posix_madvise did not succeed");
	}
    }
#endif
}

MmapIOWrapper::~MmapIOWrapper() {
    if (munmap(map_addr, map_length) == -1) {
        perror("munmap");
        crash("IOWrapper: could not munmap");
    }
    if (close(fd) != 0) {
        perror("close");
        crash(std::string("could not close buffer file ") + bufferFilename);
    }
}

int MmapIOWrapper::read(void *dest, std::size_t position, std::size_t len) {
    std::memcpy(dest, map_addr + position, len);
    return 0;
}

int MmapIOWrapper::write(void *src, std::size_t position, std::size_t len) {
    std::memcpy(map_addr + position, src, len);

    uintptr_t sync_addr = (uintptr_t)map_addr + position;
    sync_addr &= ~(static_cast<uintptr_t>(mempagesize) - 1);

    std::size_t sync_len = len + ((uintptr_t)map_addr + position - sync_addr);
    
    if (msync((void*)sync_addr, sync_len, MS_SYNC) != 0) {
        perror("msync");
        crash(std::string("could not sync file ") + bufferFilename);
    }
    return 0;
}

const char* MmapIOWrapper::get_filename() const {
    return bufferFilename.c_str();
}

int MmapIOWrapper::open_flags() {
    return O_RDWR | O_CREAT;
}
