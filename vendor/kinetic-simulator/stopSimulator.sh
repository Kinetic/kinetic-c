# Ensure stray simulators are not still running
KINETIC_SIM = `ps -ef | grep kinetic-simulator | grep -v 'grep' | tr -s ' ' | cut -d ' ' -f 2`
if [! -z "$KINETIC_SIM" ]; then
  echo Killing kinetic-simulator w/ PID: $KINETIC_SIM
  kill $KINETIC_SIM
  echo Killed all kinetic simulators!
else
  echo No kinetic simulators alive to kill.
fi
