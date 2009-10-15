#!/bin/bash

# this file is mostly meant to be used by the author himself.

root=`pwd`
cd ~/work
version=$1
opts=$2
lwp-mirror "http://sysoev.ru/nginx/nginx-$version.tar.gz" nginx-$version.tar.gz
tar -xzvf nginx-$version.tar.gz
cd nginx-$version/
if [[ "$BUILD_CLEAN" -eq 1 || ! -f Makefile ]]; then
    ./configure --prefix=/opt/nginx \
          --without-http_charset_module \
          --without-http_gzip_module \
          --without-http_ssi_module \
          --without-http_userid_module \
          --without-http_access_module \
          --without-http_auth_basic_module  \
          --without-http_autoindex_module \
          --without-http_geo_module  \
          --without-http_map_module  \
          --without-http_referer_module  \
          --without-http_rewrite_module  \
          --without-http_proxy_module    \
          --without-http_fastcgi_module   \
          --without-http_memcached_module  \
          --without-http_limit_zone_module  \
          --without-http_limit_req_module  \
          --without-http_empty_gif_module  \
          --without-http_browser_module  \
          --without-http_upstream_ip_hash_module \
          --add-module=$root $opts
fi
if [ -f /opt/nginx/sbin/nginx ]; then
    rm -f /opt/nginx/sbin/nginx
fi
if [ -f /opt/nginx/logs/nginx.pid ]; then
    kill `cat /opt/nginx/logs/nginx.pid`
fi
make install -j3

