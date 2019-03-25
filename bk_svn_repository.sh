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

   PRJ=1108
   source=svn://10.12.3.233:1111/mnt/app1/ray/project/svn/can$PRJ.repository
   target=https://github.com/rayjhuang/CAN$PRJ
   echo -e "==$source\n->$target"

   if [ -d ./CAN$PRJ ]; then
      cd ./CAN$PRJ
      git svn fetch --all
      cd ..
   else
      git svn clone $source CAN$PRJ # takes long!!
   fi

   if [ ! -d ./CAN$PRJ ]; then
      echo No repository created
   else
      cd ./CAN$PRJ
      git push --force --verbose --progress --all --repo=$target
      cd ..
   fi

