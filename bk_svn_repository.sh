#!/bin/bash
##
## for en exist svn repository $target,
## clone the $source or update local (the local gets higher priority)
## create local branch(es) for those remote branch(es)
## than push all branch(es) and tag(s) to $target
##
## rayjhuang@msn.com 2019/03/25
## All Rights Are Reserved

#  REPO=4220a1;  PRJ=4220a1
#  REPO=a8;      PRJ=a8
#  REPO=CAN1108; PRJ=can1108
#  REPO=CAN1110; PRJ=can1110
#  REPO=CAN1112; PRJ=can1112
   REPO=CAN1124; PRJ=can1124

   source=svn://10.12.3.239:1111/mnt/app1/ray/project/svn/$PRJ.repository
   target=ssh://ray@10.12.3.239/mnt/app1/git/HW/$PRJ
#  target=https://github.com/rayjhuang/$REPO
   echo -e "==$source\n->$target"

   if [ -d ./$REPO ]; then
      echo -e "\t local repository found, fetching....."
      cd ./$REPO
      git svn fetch --all
      cd ..
   else
      echo -e "\t local repository not found, cloning....."
      git svn clone $source $REPO # takes long!!
   fi

   if [ -d ./$REPO ]; then
      cd ./$REPO
      git push --force --verbose --progress --all --repo=$target
      cd ..
   else
      echo No working directory to push
   fi

