#!/bin/bash

source defaults.sh

function setup_dbgen() {
    rm -rf tpch-kit || true
    git clone https://github.com/gregrahn/tpch-kit.git
    cd tpch-kit/dbgen
    make MACHINE=LINUX DATABASE=POSTGRESQL
    cd ..
    cd ..
}

function fail_if_dbgen_not_setup() {
    if ! test -e tpch-kit; then
        echo "tpch-kit has not been set up yet. This incident will be reported!"
        exit 1
    fi
}

function clean_queries() {
    rm -rf /tmp/queries || true
    mkdir /tmp/queries
}

function clean_tables() {
    rm -rf $TPCH_TABLE_TEMP_DIR || true
    mkdir $TPCH_TABLE_TEMP_DIR
}

function fake_psql() {
    echo $@
    numactl --cpunodebind=$NUMA_NODES --membind=$NUMA_NODES enroot start \
        --rw \
        -m $TPCH_TABLE_TEMP_DIR:/tmp/tables \
        -m /tmp/queries:/tmp/queries \
        $UBUNTU_CONTAINER_NAME \
        psql -h localhost -p 5432 tpch "$@"
}

function drop_database() {
    numactl --cpunodebind=$NUMA_NODES --membind=$NUMA_NODES enroot start \
        --rw \
        $UBUNTU_CONTAINER_NAME \
        dropdb -h localhost -p 5432 tpch || true
}

function create_database() {
    numactl --cpunodebind=$NUMA_NODES --membind=$NUMA_NODES enroot start \
        --rw \
        $UBUNTU_CONTAINER_NAME \
        createdb -h localhost -p 5432 tpch
}

function load_schema() {
    wget https://raw.githubusercontent.com/oltpbenchmark/oltpbench/master/src/com/oltpbenchmark/benchmarks/tpch/ddls/tpch-postgres-ddl.sql
    wget https://raw.githubusercontent.com/oltpbenchmark/oltpbench/master/src/com/oltpbenchmark/benchmarks/tpch/ddls/tpch-postgres-index-ddl.sql

    fake_psql < tpch-postgres-ddl.sql
    fake_psql < tpch-postgres-index-ddl.sql
    rm tpch-postgres-ddl.sql
    rm tpch-postgres-index-ddl.sql
}

function run_dbgen() {
    curr_dir=$(pwd)
    cp -a tpch-kit/dbgen/. $TPCH_TABLE_TEMP_DIR 

    cd $TPCH_TABLE_TEMP_DIR 

    echo "Using $DBGEN_CORES cores for generating the data"
    for i in `seq 1 $DBGEN_CORES`; do
        ./dbgen -vf -s $TPCH_SCALEFACTOR -C $DBGEN_CORES -S $i &
    done
    wait
    # fix the non-parallel tables
    ./dbgen -vf -s $TPCH_SCALEFACTOR -C $DBGEN_CORES -S 1 -T n & # fixes nation.tbl
    ./dbgen -vf -s $TPCH_SCALEFACTOR -C $DBGEN_CORES -S 1 -T r & # fixes region.tbl
    wait
    cd $curr_dir
}

function load_tables() {
    curr_dir=$(pwd)
    cd $TPCH_TABLE_TEMP_DIR 
    tables=(region.tbl nation.tbl part.tbl supplier.tbl partsupp.tbl customer.tbl orders.tbl lineitem.tbl)
    for i in ${tables[@]}; do
        table=${i/.tbl/}
        echo "Loading $table..."
        # loopen um alle dateien der table

        for table_file in $i*; do
            echo "Fixing file $table_file"
            sed -i 's/|$//' $table_file &
        done
        wait

        for table_file in $i*; do
            fake_psql -c "\\copy $table FROM '/tmp/tables/$table_file' CSV DELIMITER '|'" &
        done
        wait
    done
    cd $curr_dir
}

function fix_queries() {
    curr_dir=$(pwd)
    cd $TPCH_TABLE_TEMP_DIR 
    for i in `ls queries/*.sql`; do
        tac $i | sed '2s/;//' | tac > /tmp/$i
    done
    cd $curr_dir
}

function prepare_benchmark() {
    ./postgres.sh stop
    ./postgres.sh reset
    ./postgres.sh start
    
    fail_if_dbgen_not_setup
    clean_queries
    clean_tables
    echo "Dropping old database"
    drop_database
    echo "Creating new database"
    create_database
    echo "Loading schema"
    load_schema
    echo "Running dbgen"
    run_dbgen
    load_tables
    fix_queries
}

function stop_postgres() {
    ./postgres.sh stop
    sleep 5
}

function start_postgres() {
    ./postgres.sh start
    sleep 5
}

function prepare_from_data() {
    data_zip=$1
    query_zip=$2
    stop_postgres
    rm -rf $POSTGRES_DATA_DIR # clear data directory
    unzip $data_zip -d $POSTGRES_DATA_DIR # unzip scale-factor data package
    start_postgres # start postgres again
    echo "Waiting 30 seconds, maybe postgres will initiate recovery"
    sleep 30
    check_for_recovery
    # prepare queries
    clean_queries
    # untar queries into /tmp/queries
    unzip $query_zip -d /tmp/queries
    check_for_recovery
    restart_postgres
    sync
    check_for_recovery
    echo "VACUUM(ANALYZE, VERBOSE)" | fake_psql
    check_for_recovery
    restart_postgres
    check_for_recovery
}


function restart_postgres() {
    sync
    ./postgres.sh restart
    sync
    sleep 5
}

function check_for_recovery() {
	while [ "$(echo "SELECT pg_is_in_recovery();" | fake_psql | tail -3 | head -1 | xargs)" != "f" ]; do
		echo "Postgres is currently in recovery. Waiting for recovery to finish before continuing."
		sleep 10;
	done
	echo "Postgres is not in recovery or recovery is done. Continuing."
}

function run_benchmark() {
    restart_postgres
    echo "query,warmedup,runtime" > ${TPCH_OUTPUT}

    for i in {1..22}; do
        cd tpch-kit/dbgen
        echo "Running query ${i}"
        DSS_QUERY=/tmp/queries ./qgen $i | sed 's/limit -1//' | sed 's/day (3)/day/' > query.sql
        sync
        time=$((time (fake_psql < query.sql) > lastout) 2>&1 | grep real | awk '{print $(NF)}' | sed '/[^[:digit:].ms]/d')
        echo "This took ${time}"
        echo "${i},0,${time}" >> ${TPCH_OUTPUT}
        echo "Result: "
        echo "$(cat lastout)"

        echo "Second run of query ${i}"
        time=$((time (fake_psql < query.sql) > lastout) 2>&1 | grep real | awk '{print $(NF)}' | sed '/[^[:digit:].ms]/d')
        echo "This took ${time}"
        echo "${i},1,${time}" >> ${TPCH_OUTPUT}
        echo "Result: "
        echo "$(cat lastout)"

        cd ..
        cd ..

        echo "Query ${i} done, restarting postgres"
        restart_postgres
    done
}

function usage() {
   echo "usage: $0 (init | run | run-from-cache <pg_data.zip> | prepare | run-quick)"
}

case $1 in

    "init")
        setup_dbgen
        echo "Initialization done."
        ;;

    "prepare")
        fail_if_dbgen_not_setup
        echo "Starting benchmark preparations"
        prepare_benchmark
        echo "Benchmark preparations done"
        ;;

    "run-quick")
        fail_if_dbgen_not_setup
        echo "We did not validate whether $0 prepare was run before. Make sure that the sql files are available in /tmp/queries, otherwise this can go horribly wrong"
        echo "Running benchmark"
        run_benchmark
        echo "Benchmark completed successfully!"
        ;;
        
    "prepare-from-cache")
        if [[ $2 == "" ||  $3 == "" ]]; then
            echo "usage: "
            exit 1
        fi
        prepare_from_data $2 $3
        ;;

    "run-from-cache")
        if [[ $2 == "" ||  $3 == "" ]]; then
            echo "usage: "
            exit 1
        fi
        prepare_from_data $2 $3
        echo "Running benchmark"
        run_benchmark
        echo "Benchmark completed successfully!"
        ;;

    "run")
        fail_if_dbgen_not_setup
        echo "Starting benchmark preparations"
        prepare_benchmark
        echo "Running benchmark"
        run_benchmark
        echo "Benchmark completed successfully!"
        ;;

    *)
        usage
        exit 1
        ;;
esac
