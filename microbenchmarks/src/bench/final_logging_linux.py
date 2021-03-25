#!/bin/python3
import os, re
import subprocess
from itertools import product, cycle
import pandas as pd
import numpy as np
from time import time
from datetime import datetime

def purge(dir, pattern):
    for f in os.listdir(dir):
        if re.search(pattern, f):
            os.remove(os.path.join(dir, f))

class Test():
    def __init__(self, cmds, preparation_cmds=[], finalization_cmds=[]):
        assert isinstance(cmds, list)
        assert isinstance(preparation_cmds, list)
        assert isinstance(finalization_cmds, list)
        self.cmds = cmds
        self.preparation_cmds = preparation_cmds
        self.finalization_cmds = finalization_cmds
        self.done = False
        self.result = None
        self.runtime = 0.0

    def _execute_cmds_parallel(self, cmds):
        pids = [subprocess.Popen(cmd.split()) for cmd in cmds]
        return [pid.wait() for pid in pids]

    def run(self):
        if self.done:
            raise Exception("Test already ran!")

        #try:
        self._execute_cmds_parallel(self.preparation_cmds)

        time_start = time()

        return_codes = self._execute_cmds_parallel(self.cmds)

        self.done = True
        self.runtime = time() - time_start # seconds
        self.result = None # TODO change

        error_codes = list(filter(lambda x: x != 0, return_codes))
        if len(error_codes) > 0:
            raise Exception("Test failed! Got error codes: " + " ".join(map(str, error_codes)))

        self._execute_cmds_parallel(self.finalization_cmds)
        #except Exception as e:
        #    print("Test failed:", e)
        #    self.result = "failed"

class Parameter():
    def __init__(self, name, flag, values):
        self.name = name
        self.options = values
        self.flag = flag

    def __str__(self):
        return self.name

    def __iter__(self):
        combined = [(value, self.flag + " " + value) for value in self.options]
        return iter(combined)

class FlagParameter(Parameter):
    def __init__(self, name, flag):
        super().__init__(name, flag, [])
    def __iter__(self):
        combined = [("no",""), ("yes",self.flag)]
        return iter(combined)

class IOWrapperParameter(Parameter):
    def __init__(self):
        super().__init__("IOWrapper", "--ioengine", [  'LINUX' ])
        #super().__init__("IOWrapper", "--ioengine", [ 'MMAP', 'LIBPMEM2', 'STD'  ])
        #super().__init__("IOWrapper", "--ioengine", [ 'STD' ])

class DontNeedFlag(FlagParameter):
    def __init__(self):
        super().__init__("DontNeedFlag", "--dontneed")

class MountParameters(Parameter):
    def __init__(self, mount_combinations):
        #arguments = [" ".join(mounts[:i]) for i in range(1, len(mounts)+1)]
        #super().__init__("Mounts", "", arguments)
        super().__init__("MountDescription", "", mount_combinations)

class BufferSizeParameter(Parameter):
    def __init__(self, sizes):
        super().__init__("Buffersize", "-b", map(str, sizes))

class PageSizeParameter(Parameter):
    def __init__(self, sizes):
        super().__init__("Pagesize", "-p", map(str, sizes))

class LogEntrySizeParameter(Parameter):
    def __init__(self, sizes):
        super().__init__("LogEntrySize", "--le", map(str, sizes))

class RandomPagePoolSizeParameter(Parameter):
    def __init__(self, sizes):
        super().__init__("RandomPagePoolSize", "--randompages", map(str, sizes))

class WorkloadParameter(Parameter):
    def __init__(self, sizes):
        super().__init__("Workload", "-t", map(str, sizes))

class WriteRatioParameter(Parameter):
    def __init__(self, sizes):
        super().__init__("WriteRatio", "-w", map(str, sizes))

class PmemGranularityFlag(FlagParameter):
    def __init__(self):
        super().__init__("PmemGranularityFlag", "--pmcl")

class MmapSyncFlag(FlagParameter):
    def __init__(self):
        super().__init__("MmapSyncFlag", "--mapsync")

class BufferManagementBenchmark():
    def __init__(self, executable):
        if not os.path.isfile(executable):
            raise Exception("bufman binary does not exist.")

        self.executable = executable

    def prepare(self):
        # initialize some stuff
        self.results = []
        buffer_sizes = [2]
        self.mount_combinations = ["/mnt/nvme0/pvn", "/mnt/nvme0/pvn /mnt/nvme1/pvn /mnt/nvme2/pvn /mnt/nvme3/pvn",  "/mnt/pmem2/pvn"]
        parameters = [
            IOWrapperParameter(),
            DontNeedFlag(),
            MountParameters(self.mount_combinations),
            BufferSizeParameter(buffer_sizes), # ingored for logging
            LogEntrySizeParameter([128, 2**13]),
            RandomPagePoolSizeParameter([16]),
            WorkloadParameter([1]),
            WriteRatioParameter([0]), #ignored anyway for non-buffermanagement benchmarks
            PmemGranularityFlag(), #ignored for logging
            MmapSyncFlag() #ignored for logging
        ]
        self.parameters = parameters
        self.thread_counts = [1, 8, 16, 32, 64]
        self.num_runs_per_configuration = 3
        self.workload = "logging2"
        self.pin_to_cores = False

        self.cleanup_buffers()

    def get_numa_prefix_for_thread(self, t):
        if self.pin_to_cores == False:
            return "numactl -N 2,3 -m 2,3"
        else:
            lista = [18, 19, 20, 23, 24, 27, 28, 32, 33, 54, 55, 56, 59, 60, 63, 64, 68, 69]
            listb = [21, 22, 25, 26, 29, 30, 31, 34, 35, 57, 58, 61, 62, 65, 66, 67, 70, 71]
            t = t % (len(lista) + len(listb))
            
            if t % 2 == 0:
                core = lista[int(t / 2)]
                mem = 2
            else:
                core = listb[int(t / 2)]
                mem = 3

            return "numactl --physcpubind {} -m {}".format(core, mem)

    def execute(self):

        combinations = list(product(*self.parameters))

        print("Running {}*{} combinations!".format(len(combinations), len(self.thread_counts)))

        def isvalidcomb(comb):
            # logging specific skips
            if comb[9][1] != "":
                return False # we do not care about the mmap sync flag as we only read anyways, but we keep it to keep indices of comb
            if comb[8][1] != "":
                return False # we do not care about the pmem granularity flag as libpmemlog does not take that

            if len(comb[2][1].split()) == 1 and comb[0][0] != "LINUX_PREALLOC":
                return False

            # general validation of combination
            if comb[8][1] != "" and comb[2][1].find("pmem") == -1:
                return False
            if comb[8][1] != "" and comb[0][0] != "LIBPMEM2":
                return False
            #if comb[0][0] == "LINUX_DIRECT" and not (comb[2][1].find("nvram") == -1):
            #    return False # LINUX_DIRECT = LINUX for PMem, at least in our current setup - we test this again on the new setup
            if comb[1][1] == "": # we only want --dontneed
                return False
            if comb[9][1] != "" and comb[0][0] != "MMAP": # mmap map sync flag only makes sense for mmap io wrapper
                return False

            return True

        for comb in filter(isvalidcomb, combinations):
            comb = list(comb)
            mounts = comb[2][0].split()
            comb[2] = ("", "")
            for thread_count in self.thread_counts:
                numaprefix = "numactl -N 2,3 -m 2,3"
                # we pin to node anyways, ignore the fancy method to pin to cores for now
                command = numaprefix + " " + self.executable + " --workload " + self.workload + " " + " ".join(map(lambda x: x[1], comb))
                cmds = [command + " --suffix " + str(thread_id) + " " + mount for thread_id,mount in zip(range(thread_count), cycle(mounts))]
                total_workload = int(comb[6][0]) * thread_count
                # bandwidth = time / total_workload

                #finalization_cmds=["rm -f {}/buffer.bin.*".format(mount) for mount in mounts]
                for _ in range(self.num_runs_per_configuration):
                    test = Test(cmds)
                    test.run()
                    bandwidth = (total_workload * 1024) / test.runtime
                    row = list(map(lambda x: x[0], comb)) + [thread_count, len(mounts), test.runtime, total_workload, bandwidth]
                    if row[2].find("tmpfs") == -1:
                        row[2] = "pmem" if row[2].find("nvme") == -1 else "nvme"
                    else:
                        row[2] = "tmpfs"

                    self.results.append(row)
                    self.cleanup_buffers() # necessary for logging workload
            self._write_results()

    def _write_results(self):
        # write results to csv
        self.result_df = pd.DataFrame(self.results, columns=list(map(str, self.parameters)) + ['Threadcount', 'Mountcount', 'Time', 'Total_Workload', 'Bandwidth'])
        self.result_df.to_csv("/mnt/nvme0/maximilian.boether/paper_results/{}/result_{}.csv".format(self.workload, str(int(datetime.now().timestamp()))), index=False)

    def cleanup_buffers(self):
        for mount_combination in self.mount_combinations:
            for mount in mount_combination.split():
                purge(mount, "buffer.bin.*")

    def finalize(self):
        self._write_results()
        self.cleanup_buffers()

    def run(self):
        self.prepare()
        self.execute()
        self.finalize()

if __name__ == "__main__":
    b = BufferManagementBenchmark('../../build/bufman')
    b.run()
