#!/bin/bash

# this file is mostly meant to be used by the author himself.

root=`pwd`
cd ~/work
version=$1
opts=$2
lwp-mirror "http://sysoev.ru/nginx/nginx-$version.tar.gz" nginx-$version.tar.gz
tar -xzvf nginx-$version.tar.gz
cd nginx-$version/
if [[ "$BUILD_CLEAN" -eq 1 || ! -f Makefile \
        || "$root/config" -nt Makefile
        || "$root/util/build.sh" -nt Makefile ]]; then
    ./configure --prefix=/opt/nginx \
          --with-http_addition_module \
          --add-module=$root $opts \
          --add-module=/home/agentz/work/nginx_eval_module-1.0.1 \
          --with-debug
  #--without-http_ssi_module  # we cannot disable ssi because echo_location_async depends on it (i dunno why?!)

fi
if [ -f /opt/nginx/sbin/nginx ]; then
    rm -f /opt/nginx/sbin/nginx
fi
if [ -f /opt/nginx/logs/nginx.pid ]; then
    kill `cat /opt/nginx/logs/nginx.pid`
fi
make -j3
make install

