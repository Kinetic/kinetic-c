#! /usr/bin/env bash

# Kill any simulators running
_SIMS=`ps -ef | grep "kinetic-simulator/kinetic-simulator-" | grep -v 'grep' | sed -e 's/^ *//' -e 's/ *$//' | tr -s ' ' | cut -d ' ' -f 2`
if [[ ! -z $_SIMS ]]; then
  for sim in $_SIMS
  do
    if [[ ! -z $sim ]]; then
      echo Killing kinetic-simulator \(pid=$sim\)
      kill $sim
    fi
  done
  echo All simulators have been terminated!
fi
