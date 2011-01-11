#%switch --prefix PREFIX
#%switch --mandir MANDIR
#%switch --sysconfdir SYSCONFDIR
#%ifnswitch --prefix /usr PREFIX
#%ifnswitch --sysconfdir /etc SYSCONFDIR
#%ifnswitch --mandir $(PREFIX)/share/man MANDIR
#?V=`cat version.txt|cut -d ' ' -f 2`
#?CFLAGS = -DPREFIX=\"$(PREFIX)\" -DSYSCONFDIR=\"$(SYSCONFDIR)\"  -DMANDIR=\"$(MANDIR)\" -Wall -march=i586 -Os -DVERSION=\"$(V)\"
#?CC=gcc
#?LDFLAGS=-static
#?schedlat:	schedlat.o jelopt.o
#?	$(CC) $(LDFLAGS) -o schedlat schedlat.o jelopt.o
#?clean:
#?	rm -f *.o schedlat
#?tarball:	clean
#?	make-tarball.sh
PREFIX= /usr
SYSCONFDIR= /etc
MANDIR= $(PREFIX)/share/man
V=`cat version.txt|cut -d ' ' -f 2`
CFLAGS = -DPREFIX=\"$(PREFIX)\" -DSYSCONFDIR=\"$(SYSCONFDIR)\"  -DMANDIR=\"$(MANDIR)\" -Wall -march=i586 -Os -DVERSION=\"$(V)\"
CC=gcc
LDFLAGS=-static
schedlat:	schedlat.o jelopt.o
	$(CC) $(LDFLAGS) -o schedlat schedlat.o jelopt.o
clean:
	rm -f *.o schedlat
tarball:	clean
	make-tarball.sh
