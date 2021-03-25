#define _XOPEN_SOURCE
#include <cstdlib>
#include <cstdio>
#include <fcntl.h>
#include <string>
#include <unistd.h>

#include "iowrapper.hpp"
#include "util.hpp"

STDIOWrapper::STDIOWrapper(struct IOWrapperConfig& config) {
    bufferFilename = std::string(config.directory) + BUFFER_FILE_BASENAME + std::string(config.file_suffix);
    if ((f = std::fopen(bufferFilename.c_str(), "r+")) == NULL) {
        perror("fopen");
        crash("IOWrapper:fopen failed");
    }
    if ((fd = fileno(f)) == -1) {
        perror("fileno");
        crash("IOWrapper: fileno for some reason crashed. AP is not happy.");
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
}

STDIOWrapper::~STDIOWrapper() {
    if(std::fclose(f) == EOF) {
        perror("fclose");
        crash(std::string("could not close buffer file ") + bufferFilename);
    }
}

int STDIOWrapper::read(void *dest, std::size_t position, std::size_t len) {
    if (std::fseek(f, position, SEEK_SET) != 0) {
        perror("fseek");
        crash(std::string("could not seek file ") + bufferFilename);
    }
    std::fread(dest, sizeof(char), len, f);
    if (std::ferror(f)) {
        perror("fread");
        crash(std::string("could not read file ") + bufferFilename);
    }

    return 0;
}

int STDIOWrapper::write(void *src, std::size_t position, std::size_t len) {
    if (std::fseek(f, position, SEEK_SET) != 0) {
        perror("fseek");
        crash(std::string("could not seek file ") + bufferFilename);
    }
    if (std::fwrite(src, sizeof(char), len, f) < len) {
        perror("fwrite");
        crash(std::string("could not write to file ") + bufferFilename);
    }

    int sync_res;
#if _POSIX_SYNCHRONIZED_IO > 0
        sync_res = fdatasync(fd);
#else
        sync_res = fsync(fd);
#endif
    if (sync_res != 0) {
#if _POSIX_SYNCHRONIZED_IO > 0
        perror("fdatasync");
#else
        perror("fsync");
#endif
        crash(std::string("could not sync file ") + bufferFilename);
    }

    return 0;
}

const char* STDIOWrapper::get_filename() const {
    return bufferFilename.c_str();
}

