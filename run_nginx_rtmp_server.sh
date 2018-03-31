!#/bin/bash

./libs/_install_dir_x86_android/sbin/nginx -s stop; ./libs/_install_dir_x86_android/sbin/nginx -c "$(pwd)"/libs/3rdplibs/nginx/nginx-1.4.0/conf/nginx.conf; ps -All | grep nginx

