#!/bin/bash
##
## delete branch(es) created in create_local_branch.bsh
##
## rayjhuang@msn.com 2019/06/19
## All Rights Are Reserved

   work_branch=`git branch | grep \* | cut -d ' ' -f2`
   echo $work_branch

   for branch in `git branch --remotes \
                      | grep -Ev 'HEAD'`; do
      target_branch=`echo $branch | cut -d '/' -f2`
      if [ $work_branch != $target_branch ]; then
         git branch -D $target_branch
      fi
   done
