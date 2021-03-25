#define _XOPEN_SOURCE

#include <string>
#include <fcntl.h>
#include <unistd.h>

#include "util.hpp"
#include "iowrapper.hpp"

LinuxIOWrapper::LinuxIOWrapper(struct IOWrapperConfig& config) {
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
}

DirectLinuxIOWrapper::DirectLinuxIOWrapper(struct IOWrapperConfig& config) {
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
}

int LinuxIOWrapper::open_flags() {
    return O_RDWR | O_CREAT;
}

int DirectLinuxIOWrapper::open_flags() {
    int oflag = O_RDWR | O_CREAT;
#ifdef __linux
        oflag |= O_DIRECT;
#else
    bmlog::warning("using DirectLinuxIOWrapper, but we are not on linux! (no O_DIRECT available)");
#endif
    return oflag;
}

void LinuxIOWrapper::handle_write_error() { }

void DirectLinuxIOWrapper::handle_write_error() {
    bmlog::error("you are using O_DIRECT and write failed. check if you are using aligned memory!");
}

LinuxIOWrapper::~LinuxIOWrapper() {
    if (close(fd) != 0) {
        perror("close");
        crash(std::string("could not close buffer file ") + bufferFilename);
    }
}

int LinuxIOWrapper::read(void *dest, std::size_t position, std::size_t len) {
    if (lseek(fd, position, SEEK_SET) == -1) {
        perror("lseek");
        crash(std::string("could not seek in file ") + bufferFilename + std::string(" to position ") + std::to_string(position));
    }
    if (::read(fd, dest, len) != static_cast<ssize_t>(len)) {
        perror("read");
        crash(std::string("could not read in file ") + bufferFilename + std::string(" from position ") + std::to_string(position));
    }
    return 0;
}

int LinuxIOWrapper::write(void *src, std::size_t position, std::size_t len) {
    if (lseek(fd, position, SEEK_SET) == -1) {
        perror("lseek");
        crash(std::string("could not seek in file ") + bufferFilename + std::string(" to position ") + std::to_string(position));
    }
    if (::write(fd, src, len) == -1) {
        perror("write");
        handle_write_error();
        crash(std::string("could not write in file ") + bufferFilename + std::string(" (fd ") + std::to_string(fd) + std::string(", len ") + std::to_string(len) + std::string(") to position ") + std::to_string(position));
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

const char* LinuxIOWrapper::get_filename() const {
    return bufferFilename.c_str();
}
