src = $(wildcard *.c)
obj = $(src:.c=.o)

CC = arm-hisiv300-linux-gcc
CFLAGS = -march=armv5te -mcpu=arm926ej-s -I../libwebsockets/_install/home/yi-hack-v3/include -I../libuv-1.11.0/_install/include
LDFLAGS = -shared -L../libwebsockets/_install/home/yi-hack-v3/lib -L../libuv-1.11.0/_install/lib -lwebsockets -luv
#PREFIX = ./_install

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

all: libprotocol_yi_hack_v3_info.so libprotocol_yi_hack_v3_command.so libprotocol_yi_hack_v3_test_proxy.so

libprotocol_yi_hack_v3_info.so: $(obj)
	$(CC) -o $@ protocol_yi_hack_v3_info.o $(LDFLAGS)

libprotocol_yi_hack_v3_command.so: $(obj)
	$(CC) -o $@ protocol_yi_hack_v3_command.o $(LDFLAGS)

libprotocol_yi_hack_v3_test_proxy.so: $(obj)
	$(CC) -o $@ protocol_yi_hack_v3_test_proxy.o $(LDFLAGS)

.PHONY: clean
clean:
	rm -f $(obj) libprotocol_yi_hack_v3_info.so
	rm -f $(obj) libprotocol_yi_hack_v3_command.so
	rm -f $(obj) libprotocol_yi_hack_v3_test_proxy.so

.PHONY: install
install: libprotocol_yi_hack_v3_info.so
	mkdir -p $(DESTDIR)$(PREFIX)/plugin
	cp $< $(DESTDIR)$(PREFIX)/plugin/libprotocol_yi_hack_v3_info.so
	cp $< $(DESTDIR)$(PREFIX)/plugin/libprotocol_yi_hack_v3_command.so
	cp $< $(DESTDIR)$(PREFIX)/plugin/libprotocol_yi_hack_v3_test_proxy.so

.PHONY: uninstall
uninstall:
	rm -f $(DESTDIR)$(PREFIX)/plugin/libprotocol_yi_hack_v3_info.so
	rm -f $(DESTDIR)$(PREFIX)/plugin/libprotocol_yi_hack_v3_command.so
	rm -f $(DESTDIR)$(PREFIX)/plugin/libprotocol_yi_hack_v3_test_proxy.so
