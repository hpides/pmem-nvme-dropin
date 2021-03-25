#!/bin/bash

# switch between pmem or working dir
SETTING=local
DOCKERHUB_USER=maxiboether
TPCH_SCALEFACTOR=1
TPCC_SCALEFACTOR=1
TPCC_DURATION=600
TPCC_TERMINALS=10
TPCC_ISOLATION=TRANSACTION_READ_COMMITTED 

DIR=$(dirname "$(readlink -f "$0")") # dont change, required afterwards


TPCH_TABLE_TEMP_DIR=/mnt/nvme0/tpch_tables

if [[ $SETTING == "pmem" ]]; then
    NUMA_NODES=2,3
    POSTGRES_DATA_DIR=/mnt/pmem2/postgres_data
    POSTGRES_ETC_DIR=/mnt/pmem2/postgres_etc
    TPCC_TEMP_DIR=/mnt/pmem2/container_mount

elif [[ $SETTING == "nvme" ]]; then
    NUMA_NODES=2,3
    POSTGRES_DATA_DIR=/mnt/nvme1/postgres_data
    POSTGRES_ETC_DIR=/mnt/nvme1/postgres_etc
    TPCC_TEMP_DIR=/mnt/nvme1/container_mount
else
    NUMA_NODES=0
    POSTGRES_DATA_DIR=$DIR/postgres_data
    POSTGRES_ETC_DIR=$DIR/postgres_etc
    TPCC_TEMP_DIR=$DIR/container_mount
    TPCH_TABLE_TEMP_DIR=/tmp/tables
fi

SQSH_DIRECTORY=$DIR/sqsh_files
POSTGRES_IMAGE_FILE=$SQSH_DIRECTORY/postgres.sqsh
UBUNTU_IMAGE_FILE=$SQSH_DIRECTORY/ubuntu.sqsh
JAVA_IMAGE_FILE=$SQSH_DIRECTORY/openjdk.sqsh

export PGUSER=$(id -un)
export PGPASSWORD=tpc

POSTGRES_CONFIG=$DIR/postgresql.conf
OUTPUT_DIRECTORY=$DIR/output
TPCC_OUTPUT=$OUTPUT_DIRECTORY/tpcc_results.csv
TPCC_OUTPUT_LOG=$OUTPUT_DIRECTORY/tpcc_results.log
TPCH_OUTPUT=$OUTPUT_DIRECTORY/tpch_results.csv
PREP_FOLDER=$DIR/container_prep

POSTGRES_DH_REPO=postgres
JAVA_DH_REPO=openjdk:10
UBUNTU_DH_REPO=ubuntu
POSTGRES_CONTAINER_NAME=postgres-tpc
UBUNTU_CONTAINER_NAME=postgres-client
JAVA_CONTAINER_NAME=java-tpc

TPCH_PARALLEL_CORES=60

DBGEN_CORES=$((TPCH_SCALEFACTOR>TPCH_PARALLEL_CORES ? TPCH_PARALLEL_CORES : TPCH_SCALEFACTOR))

function get_postgres_pid() {
    enroot list -f | grep "^$POSTGRES_CONTAINER_NAME " | awk '{print $2}' | tr -d '\n'
}

function is_postgres_running() {
    existing_pid="$(get_postgres_pid)"
    if ! test -z "$existing_pid" && test -e "/proc/$existing_pid"; then
        return 0
    else
        return 1 # 1 means that postgres is not running (no success in checking if postgres is running)
    fi
}

function fail_if_postgres_is_running() {
    if is_postgres_running; then
        existing_pid="$(get_postgres_pid)"
        echo "Container still exists under pid $existing_pid!"
        exit 1
    fi
}

function fail_if_postgres_is_not_running() {
    if ! is_postgres_running; then
        echo "Postgres is not running!"
        exit 1
    fi
}

# prepare enroot on delab
HOST=$(hostname)
if [[ $HOST == nvram* ]];
then
    if [[ $SETTING == "pmem" ]]; then
        mkdir -p /mnt/pmem2/enroot_data
        export XDG_DATA_HOME=/mnt/pmem2/enroot_data
    elif [[ $SETTING == "nvme" ]]; then
        mkdir -p /mnt/nvme1/enroot_data
        export XDG_DATA_HOME=/mnt/nvme1/enroot_data
    else
        export XDG_DATA_HOME=/scratch/$(id -un)/enroot
    fi
fi

mkdir -p $SQSH_DIRECTORY
mkdir -p $OUTPUT_DIRECTORY
# download necessary image files from docker hub
if ! test -e $POSTGRES_IMAGE_FILE; then
    enroot import -o $POSTGRES_IMAGE_FILE docker://"${DOCKERHUB_USER}"@"${POSTGRES_DH_REPO}"
fi
if ! test -e $UBUNTU_IMAGE_FILE; then
    enroot import -o $UBUNTU_IMAGE_FILE docker://"${DOCKERHUB_USER}"@"${UBUNTU_DH_REPO}"
fi
if ! test -e $JAVA_IMAGE_FILE; then
    enroot import -o $JAVA_IMAGE_FILE docker://"${DOCKERHUB_USER}"@"${JAVA_DH_REPO}"
fi

# warning: this file shall not be executed!
