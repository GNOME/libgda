PERL_FILES = \
	Gnome/GDA/Common/Common.pm \
	Gnome/GDA/Common/Common.xs \
	Gnome/GDA/Common/Makefile.PL \
	Gnome/GDA/Common/test.pl \
	Gnome/GDA/Common/Changes \
	Gnome/GDA/Common/MANIFEST \
	Gnome/GDA/Client/Client.pm \
	Gnome/GDA/Client/Client.xs \
	Gnome/GDA/Client/Makefile.PL \
	Gnome/GDA/Client/test.pl \
	Gnome/GDA/Client/Changes \
	Gnome/GDA/Client/MANIFEST

EXTRA_DIST = $(PERL_FILES)

all:
	cd Gnome/GDA/Common && perl Makefile.PL && make
	cd Gnome/GDA/Client && perl Makefile.PL && make

# installation is made to /usr/local/lib/site_perl and /usr/local/man
install-data-local:
	cd Gnome/GDA/Common && make install
	cd Gnome/GDA/Client && make install
