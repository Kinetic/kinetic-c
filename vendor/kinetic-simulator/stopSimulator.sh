#! /usr/bin/env bash

# Ensure stray simulators are not still running
_SIMS=`ps -ef | grep "kinetic-simulator/kinetic-simulator-" | grep -v 'grep' | sed -e 's/^ *//' -e 's/ *$//' | tr -s ' '`
echo "Kinetic Simulators Still Alive"
echo "------------------------------"
echo $_SIMS
echo

if [[ ! -z $_SIMS ]]; then
  _KINETIC_SIM=`echo $_SIMS | cut -d ' ' -f 2`
  echo Simulator PIDs: $_KINETIC_SIM
  if [[ ! -z $_KINETIC_SIM ]]; then
    sleep 2
    echo Killing kinetic-simulator with PIDs: $_KINETIC_SIM
    kill $_KINETIC_SIM
    echo Killed all kinetic simulators!
    sleep 2
  fi
fi
