#pragma once

#include <string>
#include <cstdio>

#ifdef __linux
#include <libpmem2.h>
#include <libpmemlog.h>
#endif

#define BUFFER_FILE_BASENAME "/buffer.bin."

struct IOWrapperConfig {
    const char *directory;
    const char *file_suffix;
    bool use_fadvise;
    bool pmem_use_cacheline_granularity;
    bool use_map_sync;
    size_t logpool_size;
    bool fadv_random;
    bool fadv_sequential;
    bool madv_random;
    bool madv_sequential;
    bool mmap_populate;
};

class IOWrapper {
    public:
        virtual ~IOWrapper() {}; // virtual destructors need implementations
        virtual int read(void *dest, std::size_t position, std::size_t len) = 0;
        virtual int write(void *src, std::size_t position, std::size_t len) = 0;  
        uint32_t get_alignment();
        uintmax_t get_filesize() const;
    protected:
        virtual const char *get_filename() const = 0;
};

class LinuxIOWrapper : public IOWrapper {
    public:
        LinuxIOWrapper() = default;
        LinuxIOWrapper(struct IOWrapperConfig&);
        ~LinuxIOWrapper() override;
        int read(void *dest, std::size_t position, std::size_t len) override; 
        int write(void *src, std::size_t position, std::size_t len) override;

    protected:
        int fd;
        std::string bufferFilename;
        const char *get_filename() const override;
        virtual int open_flags();
        virtual void handle_write_error();
};

class DirectLinuxIOWrapper : public LinuxIOWrapper {
    public:
        DirectLinuxIOWrapper(struct IOWrapperConfig&);
    protected:
        int open_flags() override;
        void handle_write_error() override;
};

class MmapIOWrapper : public IOWrapper {
    public:
        MmapIOWrapper() = default;
        MmapIOWrapper(struct IOWrapperConfig&);
        ~MmapIOWrapper() override;
        int read(void *dest, std::size_t position, std::size_t len) override; 
        int write(void *src, std::size_t position, std::size_t len) override;

    protected:
        int fd;
        char *map_addr;
        int mempagesize;
        std::size_t map_length;
        std::string bufferFilename;
        virtual int open_flags();
        const char *get_filename() const override;
};

class STDIOWrapper : public IOWrapper {
    public:
        STDIOWrapper() = default;
        STDIOWrapper(struct IOWrapperConfig&);
        ~STDIOWrapper() override;
        int read(void *dest, std::size_t position, std::size_t len) override; 
        int write(void *src, std::size_t position, std::size_t len) override;

    protected:
        std::FILE *f;
        int fd;
        std::string bufferFilename;
        const char *get_filename() const override;
};

#ifdef __linux
class LibPMIOWrapper : public IOWrapper {
    public:
        LibPMIOWrapper() = default;
        LibPMIOWrapper(struct IOWrapperConfig&);
        ~LibPMIOWrapper() override;
        int read(void *dest, std::size_t position, std::size_t len) override; 
        int write(void *src, std::size_t position, std::size_t len) override;

    protected:
        int fd;
        struct pmem2_config* pmcfg;
        struct pmem2_map* pmmap;
        struct pmem2_source* pmsrc;
        pmem2_memcpy_fn pmmemcpy_fn;
        pmem2_persist_fn pmpersist_fn;
        void *map_addr;
        std::string bufferFilename;
        virtual int open_flags();
        const char *get_filename() const override;
};
#endif


class ASMIOWrapper : public IOWrapper {
    public:
        ASMIOWrapper() = default;
        ASMIOWrapper(struct IOWrapperConfig&);
        ~ASMIOWrapper() override;
        int read(void *dest, std::size_t position, std::size_t len) override; 
        int write(void *src, std::size_t position, std::size_t len) override;
    protected:
        int fd;
        char *map_addr;
        int mempagesize;
        std::size_t map_length;
        std::string bufferFilename;
        virtual int open_flags();
        const char *get_filename() const override;
};

template<typename Wrapper>
IOWrapper *create_io_wrapper(struct IOWrapperConfig& config) {
    return new Wrapper{config};
}


class AppendableFile {
    public:
        virtual ~AppendableFile() {}; // virtual destructors need implementations
        virtual int append(void *src, std::size_t len) = 0;
        uint32_t get_alignment();
        uintmax_t get_filesize() const;
        virtual void allocate(std::size_t) {};
    protected:
        virtual const char *get_filename() const = 0;
};

class LinuxAppendableFile: public AppendableFile {
    public:
        LinuxAppendableFile() = default;
        LinuxAppendableFile(struct IOWrapperConfig);
        ~LinuxAppendableFile() override;
        virtual int append(void *src, std::size_t len) override;
        virtual void allocate(std::size_t len) override;
    protected:
        int fd;
        std::string bufferFilename;
        const char *get_filename() const override;
        virtual int open_flags();
};

class LinuxPreallocatedAppendableFile: public AppendableFile {
    public:
        LinuxPreallocatedAppendableFile() = default;
        LinuxPreallocatedAppendableFile(struct IOWrapperConfig);
        ~LinuxPreallocatedAppendableFile() override;
        virtual int append(void *src, std::size_t len) override;
        virtual void allocate(std::size_t len) override;
    protected:
        int fd;
        std::string bufferFilename;
        const char *get_filename() const override;
        virtual int open_flags();
};

class LinuxPrefaultedAppendableFile: public AppendableFile {
    public:
        LinuxPrefaultedAppendableFile() = default;
        LinuxPrefaultedAppendableFile(struct IOWrapperConfig);
        ~LinuxPrefaultedAppendableFile() override;
        virtual int append(void *src, std::size_t len) override;
    protected:
        int fd;
        std::string bufferFilename;
        const char *get_filename() const override;
        virtual int open_flags();
};

#ifdef __linux
class LibpmemAppendableFile: public AppendableFile {
    public:
        LibpmemAppendableFile() = default;
        LibpmemAppendableFile(struct IOWrapperConfig);
        ~LibpmemAppendableFile() override;
        virtual int append(void *src, std::size_t len) override;
    protected:
        std::string bufferFilename;
        const char *get_filename() const override;
        PMEMlogpool *plp;
};
#endif

#ifdef __linux
class LibpmemPrefaultedAppendableFile: public AppendableFile {
    public:
        LibpmemPrefaultedAppendableFile() = default;
        LibpmemPrefaultedAppendableFile(struct IOWrapperConfig);
        ~LibpmemPrefaultedAppendableFile() override;
        virtual int append(void *src, std::size_t len) override;
    protected:
        std::string bufferFilename;
        const char *get_filename() const override;
        PMEMlogpool *plp;
};
#endif

template<typename AppendableFileType>
AppendableFile *create_appendable_file(struct IOWrapperConfig& config) {
    return new AppendableFileType{config};
}
