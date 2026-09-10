#!/bin/ksh

cp -p ./distro/out/* /bones/docker/gandolf/www/html/vclock/distro/1.0

if [ $? = 0 ];then
  echo publish success
else
  echo publish failed
fi
