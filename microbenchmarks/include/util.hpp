#pragma once

#include <cstdint>
#include <string>
#include <random>

namespace bmlog {
    void debug(const char *msg, bool flush = true);
    void info(const char *msg, bool flush = true);
    void info(std::string msg, bool flush = true);
    void warning(const char *msg, bool flush = true);
    void error(const char *msg, bool flush = true);
}

void crash(const std::string& msg);
void crash(const char *msg);

void prepare_logging();

std::mt19937 TrulyRandomEngine();

std::mt19937 CustomSeededEngine(uint32_t seed);

uint32_t suffix_to_seed(std::string suffix);

class AlignedMemoryBlock {
    public:
        AlignedMemoryBlock() = delete;
        AlignedMemoryBlock(AlignedMemoryBlock&& other);
        AlignedMemoryBlock& operator=(AlignedMemoryBlock&& other);
        explicit AlignedMemoryBlock(uint32_t alignment, std::size_t len);
        ~AlignedMemoryBlock();
        void *operator*() const;
    private:
        void *real_address;
        void *aligned_address;
};
