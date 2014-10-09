# Ensure stray simulators are not still running
ps -ef | grep kinetic-simulator | grep -v 'grep' | tr -s ' ' | cut -d ' ' -f 3 | xargs kill -9 $1
