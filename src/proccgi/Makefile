src = $(wildcard *.c)
obj = $(src:.c=.o)

CC = arm-hisiv300-linux-gcc
CFLAGS = -march=armv5te -mcpu=arm926ej-s
LDFLAGS = 
PREFIX = ./_install

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

proccgi: $(obj)
	$(CC) -o $@ $^ $(LDFLAGS)

.PHONY: clean
clean:
	rm -f $(obj) proccgi

.PHONY: install
install: proccgi
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	cp $< $(DESTDIR)$(PREFIX)/bin/proccgi

.PHONY: uninstall
uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/proccgi
