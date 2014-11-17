#! /usr/bin/env bash
BASE_DIR=`dirname "$0"`
BASE_DIR=`cd "$BASE_DIR"; pwd`

JAVA=""
if [ "$JAVA_HOME" != "" ]; then
   JAVA=$JAVA_HOME/bin/java
else
   $JAVA_HOME='/usr'
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

if ! netstat -an | grep LISTEN | grep "\*\.8123"; then
    exec "$JAVA" -classpath "$CLASSPATH" com.seagate.kinetic.simulator.internal.SimulatorRunner "$@" &
    while ! netstat -an | grep LISTEN | grep "\*\.8123"
    do sleep 1; done
fi
