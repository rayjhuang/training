#!/bin/bash

   git status
   if [[ `git status --porcelain --untracked-files no` ]]
   then
      echo DIRTY!!
   else
      echo CLEAN!!
   fi

