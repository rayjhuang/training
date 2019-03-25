#!/bin/bash
##
## for en exist repository, $source, and an exist empty one, $target,
## clone the $source into the ./temp subdirectory
## create local branch(es) for those remote branch(es)
## than push all branch(es) and tag(s) to $target
## remove ./temp
##
## rayjhuang@msn.com 2019/03/25
## All Rights Are Reserved

   source='svn://10.12.3.233:1111/mnt/app1/ray/project/svn/can1108.repository'
   target='https://github.com/rayjhuang/CAN1108'

   if [ -d ./temp ]; then
      echo Remove ./temp
      rm -rf ./temp
   fi

   git svn clone $source temp

   if [ ! -d ./temp ]; then
      echo No repository created
   else
      git push --all  --repo=$target -f
      cd ..
      rm -rf ./temp
   fi

