#!/bin/python3
import os
import subprocess
from itertools import product
import pandas as pd
import numpy as np
from time import time
from datetime import datetime

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

        try:
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
        except Exception as e:
            print("Test failed:", e)
            self.result = "failed"

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
        super().__init__("IOWrapper", "--ioengine", [ 'LINUX', 'MMAP', 'LIBPMEM2'])

class DontNeedFlag(FlagParameter):
    def __init__(self):
        super().__init__("DontNeedFlag", "--dontneed")

class MountParameters(Parameter):
    def __init__(self, mount_combinations):
        super().__init__("MountDescription", "", mount_combinations)

class BufferSizeParameter(Parameter):
    def __init__(self, sizes):
        super().__init__("Buffersize", "-b", map(str, sizes))

class PageSizeParameter(Parameter):
    def __init__(self, sizes):
        super().__init__("Pagesize", "-p", map(str, sizes))

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
        buffer_sizes = [5]
        self.mount_combinations = [ "/mnt/pmem2/pvn", "/mnt/nvme0/pvn /mnt/nvme1/pvn /mnt/nvme2/pvn /mnt/nvme3/pvn", "/mnt/nvme0/pvn"]
        parameters = [
            IOWrapperParameter(),
            DontNeedFlag(),
            MountParameters(self.mount_combinations),
            BufferSizeParameter(buffer_sizes),
            PageSizeParameter([2**12, 2**13]),
            RandomPagePoolSizeParameter([16]),
            WorkloadParameter([5]),
            WriteRatioParameter([0, 20, 80])
        ]
        self.parameters = parameters
        self.thread_counts = [1, 8, 16, 32, 64]
        self.num_runs_per_configuration = 1
        self.pin_to_cores = True

        # initialize buffers
        preparation_cmds = [self.executable + " --suffix " + str(thread_id) + " -b " + str(max(buffer_sizes)) + " --initialize " + " ".join(set(" ".join(self.mount_combinations).split())) for thread_id in range(max(self.thread_counts))]
        preparation_test = Test(preparation_cmds)
        preparation_test.run()

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
        print("Running {}*{} combinations {} times!".format(len(combinations), len(self.thread_counts), self.num_runs_per_configuration))

        def isvalidcomb(comb):
            if comb[8][1] != "" and comb[2][1].find("nvram") == -1:
                return False
            if comb[8][1] != "" and comb[0][0] != "LIBPMEM2":
                return False
            if comb[1][1] == "": # we only want --dontneed
                return False                
            return True

        for comb in filter(isvalidcomb, combinations):
            for thread_count in self.thread_counts:

                command = self.executable + " " + "--rtbs 1073741824 " + " ".join(map(lambda x: x[1], comb))
                cmds = [self.get_numa_prefix_for_thread(i) + " " + command + " --suffix " + str(i) for i in range(thread_count)]
                mounts = comb[2][0].split()
                total_workload = int(comb[6][0]) * thread_count

                for _ in range(self.num_runs_per_configuration):
                    test = Test(cmds, preparation_cmds=[cmd + " --scramble" for cmd in cmds])
                    test.run()
                    bandwidth = (total_workload * 1024) / test.runtime
                    row = list(map(lambda x: x[0], comb)) + [thread_count, len(mounts), test.runtime, total_workload, bandwidth]
                    if row[2].find("tmpfs") == -1:
                        row[2] = "pmem" if row[2].find("nvme") == -1 else "nvme"
                    else:
                        row[2] = "tmpfs"

                    self.results.append(row)
            self._write_results()

    def _write_results(self):
        # write results to csv
        self.result_df = pd.DataFrame(self.results, columns=list(map(str, self.parameters)) + ['Threadcount', 'Mountcount', 'Time', 'Total_Workload', 'Bandwidth'])
        self.result_df.to_csv("/mnt/nvme0/maximilian.boether/results/buffermanagement_nol2/result_{}.csv".format(str(int(datetime.now().timestamp()))), index=False)

    def finalize(self):
        self._write_results()

        finalization_cmds = []
        for mount_combination in self.mount_combinations:
            for mount in mount_combination.split():
                finalization_cmds.append("rm -f {}/buffer.bin.*".format(mount))

        finalization_test = Test(finalization_cmds)
        finalization_test.run()

    def run(self):
        self.prepare()
        self.execute()
        self.finalize()

if __name__ == "__main__":
    b = BufferManagementBenchmark('../../build/bufman')
    b.run()
