# TPC-H/Postgres Containerized Benchmarks
In this folder, you find a fully containerized setup of TPC-H running inside postgres. We utilize `enroot` for containers.

## Usage
You can use the `postgres.sh` script to manage postgres. As a first step, run `./postgres.sh init` to initialize all required containers.
You can use the `tpch.sh` script to manage the TPC-H benchmark.