#pragma once

#include <cstddef>
#include <functional>
#include <vector>

#include "buffer_manager.hpp"
#include "util.hpp"

class Workload {
    public:
        virtual ~Workload() {};
        virtual void run() = 0;
};

class BufferManagementWorkload: public Workload {
    public:
        BufferManagementWorkload() = delete;
        explicit BufferManagementWorkload(BufferManager& bm, std::size_t total_workload, float write_proportion, int random_pages, uint32_t pattern_seed, uint64_t target_pages);
        void run() final;
    private:
        std::mt19937& gen();
        bool do_write();
        uint64_t next_page_id();
        int next_random_pagepool_id();
        int next_random_data();
        int next_random_position(int maxval);
        BufferManager& bm;
        std::size_t total_workload;
        float write_proportion;
        uint32_t pattern_seed;
        uint64_t target_pages;
        uint64_t max_page_id;
        std::vector<AlignedMemoryBlock> random_page_pool;
};

class TableScanWorkload: public Workload {
    public:
        TableScanWorkload() = delete;
        explicit TableScanWorkload(BufferManager& bm, std::size_t total_workload);
        void run() final;
    private:
        BufferManager& bm;
        std::size_t total_workload;
};

class LoggingWorkload: public Workload {
    public:
        LoggingWorkload() = delete;
        explicit LoggingWorkload(BufferManager& bm, std::size_t total_workload, std::size_t log_entry_size, uint32_t pattern_seed, bool committing);
        void run() final;
    private:
        BufferManager& bm;
        std::size_t total_workload;
        std::size_t log_entry_size;
        uint32_t pattern_seed;
        bool committing;
        std::mt19937& gen();
        int next_random_data(int max);
};

class SimpleLoggingWorkload: public Workload {
    public:
        SimpleLoggingWorkload() = delete;
        explicit SimpleLoggingWorkload(std::string directory, std::string file_suffix, std::function<AppendableFile*(struct IOWrapperConfig&)> create_appendable_file,  std::size_t total_workload, std::size_t log_entry_size, uint32_t pattern_seed, int page_pool_size, bool log_use_fallocate);
        ~SimpleLoggingWorkload();
        void run() final;
    private:
        AppendableFile *logfile;
        std::size_t total_workload;
        std::size_t log_entry_size;
        uint32_t pattern_seed;
        int page_pool_size;
        std::mt19937& gen();
        int next_random_data(int max);
        std::vector<AlignedMemoryBlock> random_page_pool;
};

