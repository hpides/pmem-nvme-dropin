# PMem vs NVMe - Benchmark Suite

This repository contains the code for our <a href="https://hpi.de/fileadmin/user_upload/fachgebiete/rabl/publications/2021/pmem_vs_nvme_cr.pdf">DaMoN '21 paper<a/>. The code enables analyzing PMem as a drop-in replacement for NVMe SSDs. In the folder `microbenchmarks`, you find a C++ project containing the code for running database workload microbenchmarks. In the folder `tpc_postgres`, you find a suite of bash scripts managing a containerized setup of postgres and the TPC-H benchmark.

If you use this in your work, please cite us.

```bibtex
@inproceedings{boether_pmemnvme_2021,
  author = {Böther, Maximilian and Kißig, Otto and Benson, Lawrence and Rabl, Tilmann},
  title = {Drop It In Like It's Hot: An Analysis of Persistent Memory as a Drop-in Replacement for NVMe SSDs},
  booktitle = {Proceedings of the International Workshop on Data Managment on New Hardware (DAMON’21), June 20–25, 2021, Virtual Event, China},
  doi = {10.1145/3465998.3466010},
  isbn = {978-1-4503-8556-5/21/06},
  publisher = {ACM},
  year = {2021}
}
```