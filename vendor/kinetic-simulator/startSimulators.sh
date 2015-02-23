#! /usr/bin/env bash
BASE_DIR=`dirname "$0"`
BASE_DIR=`cd "$BASE_DIR"; pwd`

JAVA=""
if [ "$JAVA_HOME" != "" ]; then
   JAVA=$JAVA_HOME/bin/java
else
   JAVA_HOME='/usr'
   JAVA=`which java`
fi

#Set the classpath
if [ "$CLASSPATH" != "" ]; then
   CLASSPATH=${CLASSPATH}:$JAVA_HOME/lib/tools.jar
else
   CLASSPATH=$JAVA_HOME/lib/tools.jar
fi
for f in $BASE_DIR/*.jar; do
   CLASSPATH=${CLASSPATH}:$f
done

if [ "$NUM_SIMS" != "" ]; then
   LIMIT=$NUM_SIMS
else
   LIMIT=2
fi

let BASE_PORT=8123
let BASE_TLS_PORT=8443
SIM="com.seagate.kinetic.simulator.internal.SimulatorRunner"
new_ports=()

echo Starting $LIMIT kinetic simulators...
let index=0
while [ $index -lt $LIMIT ]; do
   let NUM=$index+1
   let PORT=$BASE_PORT+$index
   let TLS_PORT=$BASE_TLS_PORT+$index
   if ! netstat -an | grep LISTEN | grep "[\.:]$PORT"; then
      echo Starting simulator instance $NUM of $LIMIT
      new_ports=("${new_ports[@]}" "$PORT" "$TLS_PORT")
      mkdir -p ~/kinetic$NUM
      exec "$JAVA" -classpath "$CLASSPATH" $SIM "$@" -port $PORT -tlsport $TLS_PORT -home ~/kinetic$NUM &> ~/kinetic$NUM/kinetic-sim.log &
   fi
   let index=index+1
done

for port in "${new_ports[@]}"; do
   echo Waiting for simulator on port $port
   while ! netstat -an | grep LISTEN | grep "[\.:]$PORT"
   do sleep 1; done
done

echo
echo Successfully started $LIMIT kinetic simulators
