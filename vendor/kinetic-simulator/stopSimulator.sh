# Ensure stray simulators are not still running
ps -ef | grep kinetic-simulator | grep -v 'grep' | tr -s ' ' | cut -d ' ' -f 2 | xargs kill $1
echo Killed all kinetic simulators!
