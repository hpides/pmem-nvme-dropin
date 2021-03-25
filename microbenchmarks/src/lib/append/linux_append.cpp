#include <cstddef>
#include <string>
#include <fcntl.h>
#include <unistd.h>
#include <filesystem>

#include "iowrapper.hpp"
#include "util.hpp"

LinuxAppendableFile::~LinuxAppendableFile() {
    if (close(fd) != 0) {
        perror("close");
        crash(std::string("could not close buffer file ") + bufferFilename);
    }
}

LinuxAppendableFile::LinuxAppendableFile(struct IOWrapperConfig config) {
    bufferFilename = std::string(config.directory) + BUFFER_FILE_BASENAME + std::string(config.file_suffix);

    if (std::filesystem::exists(std::filesystem::path(bufferFilename))) {
        std::remove(bufferFilename.c_str());
        bmlog::warning("deleting pre-existing log file, please remove it before executing the workload next time!");
    }

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
#endif
}

int LinuxAppendableFile::append(void * src, std::size_t len) {
    if (::write(fd, src, len) == -1) {
        perror("write");
        crash(std::string("could not append to file ") + bufferFilename + std::string(" (fd ") + std::to_string(fd) + std::string(", len ") + std::to_string(len));
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

void LinuxAppendableFile::allocate(std::size_t len) {

#ifdef __linux
    if (posix_fallocate(fd, 0, len) != 0) {
        perror("posix_fallocate");
        crash(std::string("could not allocate for ") + bufferFilename + " a total of " + std::to_string(len) + " bytes");
    }
#else
    (void)len;
    bmlog::warning("posix_fallocate is not available on this system");
#endif

}

int LinuxAppendableFile::open_flags() {
    return O_CREAT | O_WRONLY | O_APPEND;
}

const char* LinuxAppendableFile::get_filename() const {
    return bufferFilename.c_str();
}
