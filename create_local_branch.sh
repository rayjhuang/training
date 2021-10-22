#!/bin/bash
##
## create local branch by remote branch's name
## because git don't push remote branch, so here create locally and then push to remote
##
## rayjhuang@msn.com 2019/06/19
## All Rights Are Reserved

   work_branch=`git branch | grep \* | cut -d ' ' -f2`
   echo $work_branch

   for branch in `git branch --remotes \
                      | grep -Ev 'HEAD'`; do
      target_branch=`echo $branch | cut -d '/' -f2`
      if [ $work_branch != $target_branch ]; then
         git checkout $branch -B $target_branch --no-track
      fi
   done

   git checkout $work_branch

