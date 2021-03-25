#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <filesystem>

#include "iowrapper.hpp"

uint32_t IOWrapper::get_alignment() {
    struct stat fstat;
    stat(get_filename(), &fstat);
    uint32_t blksize = static_cast<uint32_t>(fstat.st_blksize);

    return blksize;
}

uintmax_t IOWrapper::get_filesize() const {
    std::filesystem::path p{get_filename()};
    return std::filesystem::file_size(p);
}

