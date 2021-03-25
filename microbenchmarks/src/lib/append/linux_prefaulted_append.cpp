#include <cstddef>
#include <string>
#include <fcntl.h>
#include <unistd.h>
#include <filesystem>

#include "iowrapper.hpp"
#include "util.hpp"

LinuxPrefaultedAppendableFile::~LinuxPrefaultedAppendableFile() {
    if (close(fd) != 0) {
        perror("close");
        crash(std::string("could not close buffer file ") + bufferFilename);
    }
}

LinuxPrefaultedAppendableFile::LinuxPrefaultedAppendableFile(struct IOWrapperConfig config) {
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
#endif

    // seek to 0
    if (lseek(fd, 0, SEEK_SET) == -1) {
        perror("lseek");
        crash("seeking to the beginning of the logfile failed!");
    }
}

int LinuxPrefaultedAppendableFile::append(void * src, std::size_t len) {

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

int LinuxPrefaultedAppendableFile::open_flags() {
    return O_CREAT | O_RDWR;
}

const char* LinuxPrefaultedAppendableFile::get_filename() const {
    return bufferFilename.c_str();
}
