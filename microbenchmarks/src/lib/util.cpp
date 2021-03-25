#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string>
#include <random>
#include <numeric>
#include <functional>
#include <utility>

#include "util.hpp"
#include "termcolor/termcolor.h"

using namespace termcolor;

void bmlog::debug(const char *msg, bool flush) {
    std::cout << grey << msg << reset << '\n';
    if (flush) std::cout << std::flush;
}

void bmlog::info(const char *msg, bool flush) {
    std::cout << white << msg << reset << '\n';
    if (flush) std::cout << std::flush;
}

void bmlog::info(std::string msg, bool flush) {
    std::cout << white << msg << reset << '\n';
    if (flush) std::cout << std::flush;
}

void bmlog::warning(const char *msg, bool flush) {
    std::cout << yellow << msg << reset << '\n';
    if (flush) std::cout << std::flush;
}

void bmlog::error(const char *msg, bool flush) {
    std::cout << red << msg << reset << '\n';
    if (flush) std::cout << std::flush;
}

void crash(const std::string& str) {
    crash(str.c_str());
}

void crash(const char *msg) {
    bmlog::error(msg);
    std::exit(EXIT_FAILURE);
}

void prepare_logging() {
    std::cout << termcolor::reset;
}

std::mt19937 TrulyRandomEngine() {
    std::vector<uint32_t> random_data(std::mt19937::state_size);
    std::random_device source;
    std::generate(random_data.begin(), random_data.end(), std::ref(source));
    std::seed_seq seeds(random_data.begin(), random_data.end());
    std::mt19937 engine(seeds);

    return engine;
}

std::mt19937 CustomSeededEngine(uint32_t seed) {
    std::vector<uint32_t> random_data(std::mt19937::state_size);
    std::iota(random_data.begin(), random_data.end(), seed);
    std::seed_seq seeds(random_data.begin(), random_data.end());
    std::mt19937 engine(seeds);

    return engine;
}

uint32_t suffix_to_seed(std::string suffix) {
    return static_cast<uint32_t>(std::hash<std::string>()(suffix));
}

AlignedMemoryBlock::AlignedMemoryBlock(uint32_t alignment, std::size_t len) {
    if (__builtin_popcount(alignment) != 1) crash("please only align zweierpotenzen");
    int align = alignment-1;

    real_address = std::malloc(static_cast<int>(len+align));
    if (real_address == NULL) crash("aligned memory allocation failed");
    aligned_address = (void*) (((uintptr_t)real_address+align)&~((uintptr_t)align));
}

AlignedMemoryBlock::AlignedMemoryBlock(AlignedMemoryBlock&& other) {
    real_address = other.real_address;
    aligned_address = other.aligned_address;
    other.real_address = nullptr;
}

AlignedMemoryBlock& AlignedMemoryBlock::operator=(AlignedMemoryBlock&& other) {
    if (this != &other) {
        real_address = other.real_address;
        aligned_address = other.aligned_address;
        other.real_address = nullptr;
    }
    return *this;
}

AlignedMemoryBlock::~AlignedMemoryBlock() {
    if (real_address)
        std::free(real_address);
}

void *AlignedMemoryBlock::operator*() const {
    return aligned_address;
}


