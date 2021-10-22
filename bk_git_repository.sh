#!/bin/bash
##
## for en exist repository $target,
## clone the $source or update local (the local gets higher priority)
## create local branch(es) for those remote branch(es)
## than push all branch(es) and tag(s) to $target
##
## rayjhuang@msn.com 2019/03/15
## All Rights Are Reserved

## REPO="CY2518";    PRJ="CAN1108/emarker"
## REPO="CY2311_R2"; PRJ="CAN1110/$REPO"
   REPO="CY2332_R0"; PRJ="CAN1112/$REPO"

   source="ssh://git@10.12.3.237:/mnt/app1/git/FW/$PRJ.git"
   target="https://github.com/rayjhuang/$REPO"
   echo -e "==$source\n->$target"

   if [ ! -d ./$REPO ]; then
      git clone $source ./$REPO
   fi
   if [ ! -d ./$REPO ]; then
      echo No working copy found
   else
      cd ./$REPO
      git branch -a
      source ../create_local_branch.sh

      git push --all  --repo=$target -f
      git push --tags --repo=$target -f

      source ../delete_created_branch.sh
      cd ..
   fi

#  rm -rf ./temp

