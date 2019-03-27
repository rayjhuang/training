#!/bin/bash
##
## for en exist repository $target,
## clone the $source or update local (the local gets higher priority)
## create local branch(es) for those remote branch(es)
## than push all branch(es) and tag(s) to $target
##
## rayjhuang@msn.com 2019/03/25
## All Rights Are Reserved

   REPO=CAN1108; PRJ=can1108
## REPO=CAN1112; PRJ=can1112

   source=svn://10.12.3.233:1111/mnt/app1/ray/project/svn/$PRJ.repository
   target=https://github.com/rayjhuang/$REPO
   echo -e "==$source\n->$target"

   if [ ! -d ./$REPO ]; then
      git svn clone $source $REPO # takes long!!
   else
      cd ./$REPO
      git svn fetch --all
      cd ..
   fi

   if [ ! -d ./$REPO ]; then
      echo No repository created
   else
      cd ./$REPO
      git push --force --verbose --progress --all --repo=$target
      cd ..
   fi

