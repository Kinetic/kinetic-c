#! /usr/bin/env bash

# Configure to launch java simulator
SIM_JARS_PREFIX=vendor/kinetic-java/kinetic-simulator-0.7.0.2-kinetic-proto-2.0.6-SNAPSHOT
CLASSPATH=${JAVA_HOME}/lib/tools.jar:${SIM_JARS_PREFIX}-jar-with-dependencies.jar:${SIM_JARS_PREFIX}-sources.jar:${SIM_JARS_PREFIX}.jar
SIM_RUNNER=com.seagate.kinetic.simulator.internal.SimulatorRunner
SIM_ADMIN=com.seagate.kinetic.admin.cli.KineticAdminCLI
BASE_DIR=`dirname "$0"`/../..
BASE_DIR=`cd "$BASE_DIR"; pwd`

echo BASE ${BASE_DIR}

#Set the classpath
if [ "$CLASSPATH" != "" ]; then
   CLASSPATH=${CLASSPATH}:$JAVA_HOME/lib/tools.jar
else
   CLASSPATH=$JAVA_HOME/lib/tools.jar
fi

for f in ./vendor/kinetic-java/*.jar; do
   CLASSPATH=${CLASSPATH}:$f
done

exec java -classpath \"${CLASSPATH}\" ${SIM_RUNNER} \"$@\" &
echo $! > pid.log
sleep 5

java -classpath \"${CLASSPATH}\" ${SIM_ADMIN} -setup -erase true

UTIL=./bin/kinetic-c-client-util
${UTIL} noop
${UTIL} put
${UTIL} get

cat pid.log | xargs kill $1
