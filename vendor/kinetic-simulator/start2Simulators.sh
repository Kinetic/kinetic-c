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

if ! netstat -an | grep LISTEN | grep "[\.:].8123"; then
    exec "$JAVA" -classpath "$CLASSPATH" com.seagate.kinetic.simulator.internal.SimulatorRunner "$@" -port 8123 -tlsport 8443 -home ~/kinetic1 &  
fi

if ! netstat -an | grep LISTEN | grep "[\.:].8124"; then
    exec "$JAVA" -classpath "$CLASSPATH" com.seagate.kinetic.simulator.internal.SimulatorRunner "$@" -port 8124 -tlsport 8444 -home ~/kinetic2 &
fi

# if ! netstat -an | grep LISTEN | grep "[\.:].8125"; then
#     exec "$JAVA" -classpath "$CLASSPATH" com.seagate.kinetic.simulator.internal.SimulatorRunner "$@" -port 8125 -tlsport 8445 -home ~/kinetic3 &
# fi

# if ! netstat -an | grep LISTEN | grep "[\.:].8126"; then
#     exec "$JAVA" -classpath "$CLASSPATH" com.seagate.kinetic.simulator.internal.SimulatorRunner "$@" -port 8126 -tlsport 8446 -home ~/kinetic4 &
# fi

while ! netstat -an | grep LISTEN | grep "[\.:]8123"
do sleep 1; done
while ! netstat -an | grep LISTEN | grep "[\.:]8443"
do sleep 1; done

while ! netstat -an | grep LISTEN | grep "[\.:]8124"
do sleep 1; done
while ! netstat -an | grep LISTEN | grep "[\.:]8444"
do sleep 1; done

# while ! netstat -an | grep LISTEN | grep "[\.:]8125"
# do sleep 1; done
# while ! netstat -an | grep LISTEN | grep "[\.:]8445"
# do sleep 1; done

# while ! netstat -an | grep LISTEN | grep "[\.:]8126"
# do sleep 1; done
# while ! netstat -an | grep LISTEN | grep "[\.:]8446"
# do sleep 1; done
