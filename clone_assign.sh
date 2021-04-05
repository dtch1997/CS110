#!/bin/bash

ASSIGNNO=$1
COMMAND="git clone /usr/class/cs110/repos/assign$ASSIGNNO/$USER ~/CS110/assign${ASSIGNNO}"

eval $COMMAND
