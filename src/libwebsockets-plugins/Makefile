src = $(wildcard *.c)
obj = $(src:.c=.o)

CC = arm-hisiv300-linux-gcc
CFLAGS = -march=armv5te -mcpu=arm926ej-s -I../libwebsockets/_install/home/app/include -I../libuv/_install/include
LDFLAGS = -shared -L../libwebsockets/_install/home/app/lib -L../libuv/_install/lib -lwebsockets -luv
#PREFIX = ./_install

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

libprotocol_yi_hack_v3_info.so: $(obj)
	$(CC) -o $@ $^ $(LDFLAGS)

.PHONY: clean
clean:
	rm -f $(obj) libprotocol_yi_hack_v3_info.so

.PHONY: install
install: libprotocol_yi_hack_v3_info.so
	mkdir -p $(DESTDIR)$(PREFIX)/plugin
	cp $< $(DESTDIR)$(PREFIX)/plugin/libprotocol_yi_hack_v3_info.so

.PHONY: uninstall
uninstall:
	rm -f $(DESTDIR)$(PREFIX)/plugin/libprotocol_yi_hack_v3_info.so
