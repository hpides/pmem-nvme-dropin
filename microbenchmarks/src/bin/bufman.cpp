#include <algorithm>
#include <cstring>
#include <cstdint>
#include <iterator>
#include <random>
#include <vector>
#include <memory>

#include <unistd.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <fcntl.h>

#include "argh.h"
#include "util.hpp"
#include "workload.hpp"
#include "buffer_manager.hpp"
#include "iowrapper.hpp"

int main(int argc, char *argv[]) {
    // setup logging IO
    prepare_logging();

    // argument parsing
    argh::parser cmdl;
    cmdl.add_params({"-l", "--workload", "-i", "--ioengine", "-b", "--buffersize", "-s", "--suffix", "-p", "--pagesize", "-w", "--write", "-t", "--total", "--randompages", "--le", "--rtbs", "--read-target-buffer-size"});
    cmdl.parse(argc, argv);

    std::size_t page_size; // B
    cmdl({"-p", "--pagesize"}, 4096) >> page_size;

    std::size_t buffer_size; // GiB
    cmdl({"-b", "--buffersize"}, 1) >> buffer_size;
    buffer_size <<= 30; // now its in B
    std::size_t full_buffer_size_argument = buffer_size;
    buffer_size /= std::distance(cmdl.pos_args().begin() + 1, cmdl.pos_args().end());

    std::string buffer_file_suffix;
    cmdl({"-s", "--suffix"}) >> buffer_file_suffix;
    std::size_t pages_per_buffer = buffer_size / page_size;

    std::string ioengine;
    cmdl({"-i", "--ioengine"}, "LINUX") >> ioengine;

    int write_proportion; // int from 0 to 100
    cmdl({"-w", "--write"}, 50) >> write_proportion;

    if (write_proportion < 0 || write_proportion > 100)
        crash("invalid write proportion " + std::to_string(write_proportion));


    std::size_t total_workload; //GiB
    cmdl({"-t", "--total"}, 8) >> total_workload;
    total_workload <<= 30;

    int random_pages;
    cmdl({"--randompages"}, 5) >> random_pages;
    
    bool initialize = false;
    if (cmdl[{"--initialize"}]) initialize = true;
    
    bool scramble = false;
    if (cmdl[{"--scramble"}]) scramble = true;

    bool use_fadvise_dontneed = false;
    if (cmdl[{"--dontneed"}]) use_fadvise_dontneed = true;

    bool pmem_use_cacheline_granularity = false;
    if (cmdl[{"--pmcl"}]) pmem_use_cacheline_granularity = true;

    bool mmap_use_map_sync = false;
    if (cmdl[{"--mapsync"}]) mmap_use_map_sync = true;

    bool log_use_fallocate = false;
    if (cmdl[{"--logging-fallocate"}]) log_use_fallocate = true;

    bool fadv_random = false;
    if (cmdl[{"--fadv-random"}]) fadv_random = true;

    bool fadv_sequential = false;
    if (cmdl[{"--fadv-sequential"}]) fadv_sequential = true;

    std::string _workload;
    cmdl({"-l", "--workload"}, "bufman") >> _workload;

    std::size_t log_entry_size; // B
    cmdl({"--le"}, 128) >> log_entry_size;

    std::size_t read_target_buffer_size;
    cmdl({"--rtbs", "--read-target-buffer-size"}, 1) >> read_target_buffer_size;
    uint64_t target_pages = std::max(static_cast<std::size_t>(1), read_target_buffer_size / page_size);

    bool mmap_populate = false;
    if (cmdl[{"--mmap-populate"}]) mmap_populate = true;

    bool madv_random = false;
    if (cmdl[{"--madv-random"}]) madv_random = true;

    bool madv_sequential = false;
    if (cmdl[{"--madv-sequential"}]) madv_sequential = true;

    std::vector<std::string> directories;
    std::copy(cmdl.pos_args().begin() + 1, cmdl.pos_args().end(), std::back_inserter(directories));
    if (cmdl.pos_args().begin() + 1 == cmdl.pos_args().end()) {
        crash("no mounts given");
    }

    bool committing = false;
    if (cmdl({"--committing"})) committing = true;

    bmlog::info("Starting up benchmark");

    bmlog::info(std::string("Workload: ") + _workload);
    bmlog::info(std::string("File Suffix: ") + buffer_file_suffix);
    bmlog::info(std::string("Pagesize: ") + std::to_string(page_size));
    bmlog::info(std::string("Buffersize on Disk: ") + std::to_string(buffer_size));
    bmlog::info(std::string("Pages per buffer: ") + std::to_string(pages_per_buffer));
    bmlog::info(std::string("Ioengine: " + ioengine));
    bmlog::info(std::string("Write Proportion: " + std::to_string(write_proportion)));
    bmlog::info(std::string("Total Workload: " + std::to_string(total_workload)));
    bmlog::info(std::string("Random Page Poolsize: " + std::to_string(random_pages)));
    bmlog::info(std::string("Read Target Pages: 0" + std::to_string(target_pages)));
    bmlog::info(std::string("Using map_sync for mmap: " + std::to_string(mmap_use_map_sync)));
    bmlog::info(std::string("FADV Random: " + std::to_string(fadv_random)));
    bmlog::info(std::string("FADV Sequential: " + std::to_string(fadv_sequential)));
    bmlog::info(std::string("MADV Random: " + std::to_string(madv_random)));
    bmlog::info(std::string("MADV Sequential: " + std::to_string(madv_sequential)));
    if (use_fadvise_dontneed) bmlog::info(std::string("fadvise_dontneed: enabled"));

    if (initialize) {
        bmlog::info("We initialize instead of benchmark.");
    } else if (scramble) {
        bmlog::info("We just scramble up data...");
    } else {
        bmlog::info("We don't initialize and just benchmark.");
    }
    bmlog::info("Mounts:");
    for (auto& dir : directories)
        bmlog::info(dir);

    bmlog::info("");
    
    std::function<IOWrapper*(struct IOWrapperConfig&)> io_wrapper_factory;
    std::function<AppendableFile*(struct IOWrapperConfig&)> appendable_file_factory = create_appendable_file<LinuxAppendableFile>;

    if (ioengine == "LINUX") {
        io_wrapper_factory = create_io_wrapper<LinuxIOWrapper>;
    } else if (ioengine == "LINUX_DIRECT") {
        io_wrapper_factory = create_io_wrapper<DirectLinuxIOWrapper>;
    } else if (ioengine == "LINUX_PREALLOC") {
        io_wrapper_factory = create_io_wrapper<LinuxIOWrapper>;
        appendable_file_factory = create_appendable_file<LinuxPreallocatedAppendableFile>;
    } else if (ioengine == "LINUX_PREFAULT") {
        io_wrapper_factory = create_io_wrapper<LinuxIOWrapper>;
        appendable_file_factory = create_appendable_file<LinuxPrefaultedAppendableFile>;
    } else if (ioengine == "MMAP") {
        io_wrapper_factory = create_io_wrapper<MmapIOWrapper>;
    } else if (ioengine == "STD") {
        io_wrapper_factory = create_io_wrapper<STDIOWrapper>;
#ifdef __linux
    } else if (ioengine == "LIBPMEM2" || ioengine == "LIBPMEM") {
        io_wrapper_factory = create_io_wrapper<LibPMIOWrapper>;
        appendable_file_factory = create_appendable_file<LibpmemAppendableFile>;
    } else if (ioengine == "LIBPMEM2_PF" || ioengine == "LIBPMEM_PF") {
        io_wrapper_factory = create_io_wrapper<LibPMIOWrapper>;
        appendable_file_factory = create_appendable_file<LibpmemPrefaultedAppendableFile>;
#endif
    } else if (ioengine == "ASM") {
        io_wrapper_factory = create_io_wrapper<ASMIOWrapper>;
    } else {
        crash("Unsupported ioengine!");
    }

    if (!initialize && !scramble) {
        std::unique_ptr<Workload> wl;
        if (_workload != "logging2") {
            BufferManager bm(directories, buffer_file_suffix.c_str(), page_size, pages_per_buffer, use_fadvise_dontneed, pmem_use_cacheline_granularity, mmap_use_map_sync, io_wrapper_factory, fadv_random, fadv_sequential, madv_random, madv_sequential, mmap_populate);
            if (_workload == "bufman") {
                wl = std::make_unique<BufferManagementWorkload>(bm, total_workload, static_cast<double>(write_proportion) / 100.0f, random_pages, suffix_to_seed(buffer_file_suffix), target_pages);
            } else if (_workload == "tablescan") {
                wl = std::make_unique<TableScanWorkload>(bm, total_workload);
            } else if (_workload == "logging") {
                if (total_workload < log_entry_size) {
                    crash("total workload requested is smaller than one single log entry!");
                }
                wl = std::make_unique<LoggingWorkload>(bm, total_workload, log_entry_size, suffix_to_seed(buffer_file_suffix), committing);
            } else {
                crash("Unsupported workload!");
            }
            
	    wl->run();
        } else {
            // LOGGING2 - das etwas andere Kind
            if (directories.size() > 1)
                bmlog::warning("You gave more than one directory for logging2. Just using the first directory!");
                
            wl = std::make_unique<SimpleLoggingWorkload>(directories[0], buffer_file_suffix, appendable_file_factory, total_workload, log_entry_size, suffix_to_seed(buffer_file_suffix), random_pages, log_use_fallocate);

	    wl->run();
        }
       
    } else if (scramble) {
        for (std::string dir : directories) {
            int child_pid = fork();
            if (child_pid == 0) {
                bmlog::info("scramble thread opened...");
                std::string buffile = dir + BUFFER_FILE_BASENAME + buffer_file_suffix;
                int fd;
                if ((fd = open(buffile.c_str(), O_RDWR)) == -1) {
                    perror("open");
                    crash("(scramble) could not open buffer file " + buffile);
                }
                char *buf = (char*)mmap(NULL, buffer_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
                if (buf == MAP_FAILED) {
                    perror("mmap");
                    crash("(scramble) mmap failed...");
                }

                // scramble stuff
                std::random_device rd;
                std::mt19937 g(rd());
                constexpr int incs = 64;
                std::uniform_int_distribution<unsigned char> pos_dis(0, incs-1);
                std::uniform_int_distribution<unsigned char> val_dis(0, 255);
                for (std::size_t i = 0; i < buffer_size; i+=incs) {
                    int pos = pos_dis(g);
                    buf[i+pos] = val_dis(g);
                }
                
                if (munmap(buf, buffer_size) == -1) {
                    perror("munmap");
                    crash("(scramble) munmap failed...");
                }

                close(fd);
                bmlog::info("scramble thread closed...");
                return 0;
            }
        }
        int status = 0;
        while (wait(&status) > -1);
    } else {
        for (std::string dir : directories) {
            pid_t child_pid = fork();
            if (child_pid == -1) {
                perror("fork");
                crash("could not fork");
            }
            if (child_pid == 0) { // we are the child
                std::size_t buf_mb = full_buffer_size_argument >> 20; // conversion to MiB
                bmlog::info(std::string("Initializing a buffer with ") + std::to_string(buf_mb) + " MiB");
                std::string of_arg = std::string("of=") + dir + BUFFER_FILE_BASENAME + buffer_file_suffix;
                std::string count_arg = std::string("count=") + std::to_string(buf_mb);
                if (execlp("/bin/dd", "/bin/dd", "if=/dev/urandom", of_arg.c_str(), "bs=1M", count_arg.c_str(), NULL) == -1) {
                    perror("(child) execlp");
                    crash("(child) Could not execute dd!");
                }
            }
        }
        int status = 0;
        while (wait(&status) > -1);
    }

    bmlog::info("Stopped buffer management benchmark");
    return 0;
}
