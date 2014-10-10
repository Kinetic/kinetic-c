#! /usr/bin/env bash
BASE_DIR=`dirname "$0"`
BASE_DIR=`cd "$BASE_DIR"; pwd`

JAVA=""
if [ "$JAVA_HOME" != "" ]; then
   JAVA=$JAVA_HOME/bin/java
else
   JAVA_HOME=/usr
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

exec "$JAVA" -classpath "$CLASSPATH" com.seagate.kinetic.admin.cli.KineticAdminCLI -instanterase
