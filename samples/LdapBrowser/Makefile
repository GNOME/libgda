CFLAGS = -Wall -g -DGDA_DISABLE_DEPRECATED `pkg-config --cflags libgda-ui-5.0`
LDFLAGS = `pkg-config --libs libgda-ui-5.0`

all: ldap-browser

ldap-browser: ldap-browser.c
	$(CC) -o ldap-browser ldap-browser.c $(CFLAGS) $(LDFLAGS)

clean:
	rm -f *~
	rm -f *.o
	rm -f ldap-browser
