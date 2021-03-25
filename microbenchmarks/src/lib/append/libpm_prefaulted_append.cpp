
#ifdef __linux

#include <cstddef>
#include <string>
#include <cstdio>
#include <filesystem>

#include <libpmemlog.h>

#include "iowrapper.hpp"
#include "util.hpp"

LibpmemPrefaultedAppendableFile::LibpmemPrefaultedAppendableFile(struct IOWrapperConfig config) {
    bufferFilename = std::string(config.directory) + BUFFER_FILE_BASENAME + std::string(config.file_suffix);
    
    plp = pmemlog_open(bufferFilename.c_str());

	if (plp == NULL) {
		perror("pmemlog_open");
		crash("Could not open pmemlog logfile");
	}

    // rewind to the beginning of the log
    pmemlog_rewind(plp);
}

LibpmemPrefaultedAppendableFile::~LibpmemPrefaultedAppendableFile() {
    pmemlog_close(plp);
}

int LibpmemPrefaultedAppendableFile::append(void * src, std::size_t len) {
    if (pmemlog_append(plp, src, len) != 0) {
        perror("pmemlog_append");
        crash("pmemlog_append failed");
    }

    return 0;
}

const char* LibpmemPrefaultedAppendableFile::get_filename() const {
    return bufferFilename.c_str();
}

#endif

