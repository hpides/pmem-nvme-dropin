#!/bin/bash
source defaults.sh

function usage() {
   echo "usage: $0 (init | init-client | init-server | reset | start | stop | restart | status)"
}

function start_postgres() {
    export ENROOT_LOGIN_SHELL=false

    numactl --cpunodebind=$NUMA_NODES --membind=$NUMA_NODES enroot start \
        --env POSTGRES_USER=$PGUSER \
        --env POSTGRES_PASSWORD=$PGPASSWORD \
        --env TERM=xterm \
        -m $POSTGRES_ETC_DIR:/etc/postgresql \
        -m $POSTGRES_DATA_DIR:/var/lib/postgresql/data \
        --rw \
        "$POSTGRES_CONTAINER_NAME" \
        postgres -c 'config_file=/etc/postgresql/postgresql.conf' &
}

function start_postgres_and_disown() {
    start_postgres
    sleep 5
    disown
    echo "Postgres started (pid=$(get_postgres_pid))"
}

function stop_postgres() {
    enroot exec $(get_postgres_pid) pg_ctl stop
}

function prepare_directories() {
    rm -rf $POSTGRES_ETC_DIR || true 2>&1 > /dev/null
    rm -rf $POSTGRES_DATA_DIR || true 2>&1 > /dev/null

    mkdir $POSTGRES_DATA_DIR
    chmod -R 750 $POSTGRES_DATA_DIR
    mkdir $POSTGRES_ETC_DIR
    cp $POSTGRES_CONFIG $POSTGRES_ETC_DIR/postgresql.conf
}

function remove_postgres_container() {
    yes | enroot remove "$POSTGRES_CONTAINER_NAME" || true
}

function create_postgres_container() {
    enroot create --name "$POSTGRES_CONTAINER_NAME" ${POSTGRES_IMAGE_FILE}
}

function create_client_container() {
    enroot create --name "$UBUNTU_CONTAINER_NAME" ${UBUNTU_IMAGE_FILE}
}

function prepare_container() {
    container_name=$1
    numactl --cpunodebind=$NUMA_NODES --membind=$NUMA_NODES enroot start \
        -m $PREP_FOLDER:/cp \
        -e TERM=xterm \
        --root \
        --rw \
        "$container_name" \
        bash /cp/prepare.sh -a
}

function prepare_server_container() {
    prepare_container $POSTGRES_CONTAINER_NAME
}

function prepare_client_container() {
    prepare_container $UBUNTU_CONTAINER_NAME
    numactl --cpunodebind=$NUMA_NODES --membind=$NUMA_NODES enroot start \
        -e TERM=xterm \
        --root \
        --rw \
        "$UBUNTU_CONTAINER_NAME" \
        apt install -y postgresql-client
}

function init_server() {
    fail_if_postgres_is_running
    prepare_directories
    remove_postgres_container
    create_postgres_container
    prepare_server_container
    start_postgres_and_disown
}

function remove_client_container() {
    yes | enroot remove "$UBUNTU_CONTAINER_NAME" || true
}

function init_client() {
    remove_client_container
    create_client_container
    prepare_client_container
}

case $1 in

    "start")
        fail_if_postgres_is_running
        start_postgres_and_disown
        ;;

    "stop")
        fail_if_postgres_is_not_running
        stop_postgres
        echo "Postgres stopped."
        ;;

    "status")
        fail_if_postgres_is_not_running
        enroot exec $(get_postgres_pid) pg_ctl status
        ;;

    "restart")
        if is_postgres_running; then stop_postgres; fi
        start_postgres_and_disown
        ;;

    "reset")
        fail_if_postgres_is_running
        prepare_directories
        start_postgres_and_disown
        ;;

    "init")
        fail_if_postgres_is_running
        init_server
        init_client
        ;;

    "init-client")
        init_client
        ;;

    "init-server")
        fail_if_postgres_is_running
        init_server
        ;;

    *)
        usage
        exit 1
        ;;
esac

