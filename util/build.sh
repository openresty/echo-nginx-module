#!/bin/bash

# this file is mostly meant to be used by the author himself.

root=`pwd`
cd ~/work
version=$1
opts=$2
home=~
lwp-mirror "http://sysoev.ru/nginx/nginx-$version.tar.gz" nginx-$version.tar.gz
tar -xzvf nginx-$version.tar.gz
cd nginx-$version/
if [[ "$BUILD_CLEAN" -eq 1 || ! -f Makefile \
        || "$root/config" -nt Makefile
        || "$root/util/build.sh" -nt Makefile ]]; then
    ./configure --prefix=/opt/nginx \
            --with-http_stub_status_module \
            --without-mail_pop3_module \
            --without-mail_imap_module \
            --without-mail_smtp_module \
            --without-http-cache \
            --without-http_upstream_ip_hash_module \
            --without-http_empty_gif_module \
            --without-http_memcached_module \
            --without-http_referer_module \
            --without-http_autoindex_module \
            --without-http_auth_basic_module \
            --without-http_userid_module \
          --with-http_addition_module \
          --add-module=$root/../ndk-nginx-module \
          --add-module=$root/../set-misc-nginx-module \
          --add-module=$root/../lz-session-nginx-module \
          --add-module=$root/../eval-nginx-module \
          --add-module=$root/../xss-nginx-module \
          --add-module=$root/../rds-json-nginx-module \
          --add-module=$root/../headers-more-nginx-module \
          --add-module=$root $opts \
          --with-debug
          #--add-module=$home/work/ndk \
          #--add-module=$home/work/ndk/examples/http/set_var \
          #--add-module=$root/../eval-nginx-module \
          #--add-module=/home/agentz/work/nginx_eval_module-1.0.1 \
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

