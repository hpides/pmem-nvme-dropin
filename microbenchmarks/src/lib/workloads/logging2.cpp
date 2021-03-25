#include <cstddef>
#include <cstring>
#include <random>
#include <vector>

#include "iowrapper.hpp"
#include "workload.hpp"
#include "util.hpp"


SimpleLoggingWorkload::SimpleLoggingWorkload(std::string directory, std::string file_suffix, std::function<AppendableFile*(struct IOWrapperConfig&)> create_appendable_file, std::size_t total_workload, std::size_t log_entry_size, uint32_t pattern_seed, int page_pool_size, bool log_use_fallocate) :
    total_workload(total_workload), log_entry_size(log_entry_size), pattern_seed(pattern_seed), page_pool_size(page_pool_size) {
        for (int i = 0; i < page_pool_size; i++) {
            random_page_pool.emplace_back(1, log_entry_size);
            for (unsigned int j = 0; j < log_entry_size; j++) {
                    ((char*)(*(random_page_pool[i])))[j] = static_cast<unsigned char>(next_random_data(255));
                }
            if (log_entry_size >= 16)
                strcpy(((char*)(*(random_page_pool[i]))), "TheCakeIsALie");
        }

        struct IOWrapperConfig iowrapperCfg;
        iowrapperCfg.directory = directory.c_str();
        iowrapperCfg.file_suffix = file_suffix.c_str();
        iowrapperCfg.logpool_size = total_workload + (200<<20); // 200 MiB more for metadata in case of libpmemlog

        logfile = create_appendable_file(iowrapperCfg);
        if (log_use_fallocate) logfile->allocate(total_workload);
    }

SimpleLoggingWorkload::~SimpleLoggingWorkload() {
    delete logfile;
}

void SimpleLoggingWorkload::run() {
    uint64_t processed_data = 0;
    bmlog::info("running logging.");
    while (processed_data + log_entry_size <= total_workload) {
        // select random page
        void *log_entry = *random_page_pool[next_random_data(page_pool_size-1)];

        // alter a single byte
        static_cast<char*>(log_entry)[next_random_data(log_entry_size-1)] = next_random_data(255);

        // append to logfile
        logfile->append(log_entry, log_entry_size);
        processed_data += log_entry_size;
    }
}

std::mt19937& SimpleLoggingWorkload::gen() {
    static std::mt19937 generator = CustomSeededEngine(pattern_seed);
    return generator;
}

int SimpleLoggingWorkload::next_random_data(int max) {
    std::uniform_int_distribution<int> dis(0, max);
    return dis(gen());
}
