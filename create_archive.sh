#!/bin/bash

cd ..
NAME=swap-v2-$(date +%s).tar.gz
tar czvf $NAME swap-v2/
mv $NAME archive/
echo scp Mayan:/home/jiri/ux/archive/$NAME .
