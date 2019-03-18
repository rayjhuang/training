#!/bin/bash
##
## for en exist repository, $source, and an exist empty one, $target,
## clone the $source into the ./temp subdirectory
## create local branch(es) for those remote branch(es)
## than push all branch(es) and tag(s) to $target
## remove ./temp
##
## rayjhuang@msn.com 2019/03/15
## All Rights Are Reserved
##
   source='ssh://git@10.12.3.237:/mnt/app1/git/FW//CAN1108/emarker'
   target='https://github.com/rayjhuang/CY2517'

   if [ -d ./temp ]; then
      echo Remove ./temp
      rm -rf ./temp
   fi

   git clone $source temp

   if [ ! -d ./temp ]; then
      echo No repository created
   else
      cd ./temp
      git branch -a
      for branch in `git branch -a | grep '^ *remotes\/origin\/' \
	                           | grep -v 'master' \
		   		   | grep -v 'HEAD'`; do
         local_branch=`echo $branch | cut -d '/' -f3`
         git checkout $branch -b $local_branch -f
      done
      git checkout master

      git push --all  --repo=$target -f
      git push --tags --repo=$target -f

      cd ..
      rm -rf ./temp
   fi

