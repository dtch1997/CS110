#!/bin/bash

LABNO=$1
COMMAND="git clone /afs/ir/class/cs110/repos/lab${LABNO}/shared ~/CS110/lab${LABNO}"

eval $COMMAND
