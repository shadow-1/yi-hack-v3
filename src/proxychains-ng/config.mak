CC=arm-hisiv300-linux-gcc
prefix=./_install
exec_prefix=./_install
bindir=./_install/bin
libdir=/home/app/locallib
includedir=./_install/include
sysconfdir=/home/app
USER_CFLAGS=-march=armv5te -mcpu=arm926ej-s -I/opt/hisi-linux/x86-arm/arm-hisiv300-linux/target/usr/include -L/opt/hisi-linux/x86-arm/arm-hisiv300-linux/target/usr/lib
USER_LDFLAGS=
AR=arm-hisiv300-linux-ar
RANLIB=arm-hisiv300-linux-ranlib
