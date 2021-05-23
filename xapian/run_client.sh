#!/bin/bash

if [ -z $1 ] || [ -z $2 ] || [ -z $3 ]; then
    echo "arg1 for QPS; arg2 as thread; arg3 as measure interval; arg4 (optional) as workload file"
    exit 1
fi

DIR=`dirname $0`
export LD_LIBRARY_PATH=$DIR/xapian-core-1.2.13/install/lib
DATA_ROOT=$DIR/../tailbench.inputs
NSERVERS=1
QPS=$1
PORT=3366
SERVER=172.17.0.3
#SERVER=127.0.0.1
THREAD=$2
NCLIENT=1
MEASURE_SLEEP_SEC=$3
WORKLOAD_DEC=$4

TBENCH_WORKLOAD_DEC=$WORKLOAD_DEC TBENCH_MEASURE_SLEEP_SEC=$MEASURE_SLEEP_SEC TBENCH_RANDSEED=3 TBENCH_CLIENT_THREADS=$THREAD TBENCH_SERVER=$SERVER TBENCH_SERVER_PORT=$PORT TBENCH_QPS=${QPS} TBENCH_MINSLEEPNS=5000 \
    TBENCH_TERMS_FILE=${DATA_ROOT}/xapian/terms.in \
    $DIR/xapian_networked_client

