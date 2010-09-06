#!/bin/sh
header_ver=`cat version.h 2>/dev/null |grep -w PLAYER_VERSION | sed 's/.* "//' | sed 's/"//'` ;
repo_ver=`git rev-parse --verify HEAD` ;
dirty_files=`git diff-index --name-only HEAD` ;
if ! [ -z "$dirty_files" ]; then repo_ver="${repo_ver}-dirty" ; fi
if [ "$header_ver" != "$repo_ver" ] ; then
cat > version.h <<EOF
#ifndef __IMX_CAMERA_VERSION_H__
#define __IMX_CAMERA_VERSION_H__
#define IMX_CAMERA_VERSION "$repo_ver"
#endif
EOF
fi