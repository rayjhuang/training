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
   source='https://github.com/rayjhuang/train1'
   target='https://github.com/rayjhuang/train2'

   git clone $source temp
   cd ./temp
   git branch -a
   for branch in `git branch -a | grep '^ *remotes\/origin\/' \
	                        | grep -v 'master' \
				| grep -v 'HEAD'`; do
      local_branch=`echo $branch | cut -d '/' -f3`
      git checkout $branch -b $local_branch -f
   done
   git checkout master
   git push --all  --repo=$target
   git push --tags --repo=$target
   cd ..
   rm -rf ./temp

