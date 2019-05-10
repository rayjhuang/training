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
      git clone $source ./temp
   fi
   if [ ! -d ./$REPO ]; then
      echo No repository found
   else
      cd ./$REPO
      git branch -a
      for branch in `git branch -a | grep '^ *remotes\/origin\/' \
                                   | grep -Ev 'master|HEAD'`; do
         local_branch=`echo $branch | cut -d '/' -f3`
         git checkout $branch -B $local_branch --no-track
      done

      git push --all  --repo=$target -f
      git push --tags --repo=$target -f
      cd ..
   fi

   rm -rf ./temp

