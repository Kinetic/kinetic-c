# Ensure stray simulators are not still running

# sleep 1
# _CUT_COL=''
# if [[ $OSTYPE == darwin* ]]; then
#   _CUT_COL='3'
# else
#   _CUT_COL='2'
# fi

_SIMS=`ps -ef | grep "kinetic-simulator/kinetic-simulator-" | grep -v 'grep' | sed -e 's/^ *//' -e 's/ *$//' | tr -s ' '`
echo "Kinetic Simulators Still Alive"
echo "------------------------------"
echo $_SIMS
echo

if [[ ! -z $_SIMS ]]; then
  _KINETIC_SIM=`echo $_SIMS | cut -d ' ' -f 2`
  echo Simulator PIDs: $_KINETIC_SIM
  if [[ ! -z $_KINETIC_SIM ]]; then
    echo Killing kinetic-simulator with PIDs: $_KINETIC_SIM
    kill $_KINETIC_SIM
    echo Killed all kinetic simulators!
  fi
fi
