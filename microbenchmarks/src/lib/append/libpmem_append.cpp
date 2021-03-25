
#ifdef __linux

#include <cstddef>
#include <string>
#include <cstdio>
#include <filesystem>

#include <libpmemlog.h>

#include "iowrapper.hpp"
#include "util.hpp"

LibpmemAppendableFile::LibpmemAppendableFile(struct IOWrapperConfig config) {
    bufferFilename = std::string(config.directory) + BUFFER_FILE_BASENAME + std::string(config.file_suffix);
    
    if (std::filesystem::exists(std::filesystem::path(bufferFilename))) {
        std::remove(bufferFilename.c_str());
        bmlog::warning("deleting pre-existing log file, please remove it before executing the workload next time!");
    }

    if (config.logpool_size == 0) 
        crash("Logpool Size cannot be zero!");
    
    plp = pmemlog_create(bufferFilename.c_str(), config.logpool_size, 0666);

	if (plp == NULL) {
		perror("pmemlog_create");
		crash("Could not create pmemlog logfile");
	}
}

LibpmemAppendableFile::~LibpmemAppendableFile() {
    pmemlog_close(plp);
}

int LibpmemAppendableFile::append(void * src, std::size_t len) {
    if (pmemlog_append(plp, src, len) != 0) {
        perror("pmemlog_append");
        crash("pmemlog_append failed");
    }

    return 0;
}

const char* LibpmemAppendableFile::get_filename() const {
    return bufferFilename.c_str();
}

#endif

