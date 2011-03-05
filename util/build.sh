#!/bin/bash

# this file is mostly meant to be used by the author himself.

root=`pwd`
version=$1
#opts=$2
home=~
target=$root/work/nginx

if [ ! -d ./buildroot ]; then
    mkdir ./buildroot || exit 1
fi

cd buildroot || exit 1

if [ ! -s "nginx-$version.tar.gz" ]; then
    if [ -f ~/work/nginx-$version.tar.gz ]; then
        cp ~/work/nginx-$version.tar.gz ./ || exit 1
    else
        wget "http://sysoev.ru/nginx/nginx-$version.tar.gz" -O nginx-$version.tar.gz || exit 1
    fi

    tar -xzvf nginx-$version.tar.gz || exit 1
fi

#tar -xzvf nginx-$version.tar.gz || exit 1
#cp $root/../no-pool-nginx/nginx-$version-no_pool.patch ./ || exit 1
#patch -p0 < nginx-$version-no_pool.patch || exit 1 || exit 1
#patch -p0 < ~/work/nginx-$version-rewrite_phase_fix.patch || exit 1

cd nginx-$version/

if [[ "$BUILD_CLEAN" -eq 1 || ! -f Makefile \
        || "$root/config" -nt Makefile
        || "$root/util/build.sh" -nt Makefile ]]; then
    ./configure --prefix=$target \
            --with-cc-opt="-DDEBUG_MALLOC" \
            --with-http_stub_status_module \
            --without-mail_pop3_module \
            --without-mail_imap_module \
            --without-mail_smtp_module \
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
          --add-module=$root/../eval-nginx-module \
          --add-module=$root/../xss-nginx-module \
          --add-module=$root/../rds-json-nginx-module \
          --add-module=$root/../headers-more-nginx-module \
          --add-module=$root/../lua-nginx-module \
          --add-module=$root $opts \
          --with-debug || exit 1
          #--add-module=$root/../lz-session-nginx-module \
          #--add-module=$home/work/ndk \
          #--add-module=$home/work/ndk/examples/http/set_var \
          #--add-module=$root/../eval-nginx-module \
          #--add-module=/home/agentz/work/nginx_eval_module-1.0.1 \
  #--without-http_ssi_module  # we cannot disable ssi because echo_location_async depends on it (i dunno why?!)

fi
if [ -f $target/sbin/nginx ]; then
    rm -f $target/sbin/nginx
fi
if [ -f $target/logs/nginx.pid ]; then
    kill `cat $target/logs/nginx.pid`
fi
make -j3
make install

