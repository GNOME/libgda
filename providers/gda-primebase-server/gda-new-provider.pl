#!/usr/bin/perl

$gda_base = "`gda-config --prefix`";
$argc = scalar @ARGV;

if ($argc < 2) {
	printf("Usage: gda-new-provider.pl providername your name\n");
	printf("       Please run in your providers' source destination dir!\n");
	printf("\n");
	printf("       ATTENTION: This will overwrite the following files:\n");
	printf("       .cvsinfo, Makefile.am, and ANY provider source generated\n");
	printf("       of $gda_base/share/gda/templates/*-srv*.tmpl!!!\n");
	exit 0;
}

$provider = $ARGV[0];
$provider_uc = uc $ARGV[0];
$name = $ARGV[1]; for ($i = 2; $i < $argc; $i++) { $name = "$name $ARGV[$i]"; }
$uuid = &gen_uuid();

@TEMPLATES = sort glob("$gda_base/share/gda/templates/*-srv*.tmpl");
@SOURCES = @TEMPLATES;

printf("provider: %s\n", $provider);
printf("Creator:  %s\n", $name);

&create_c_source();
&create_oaf_info();
&create_makefile_am();
&create_misc();

sub create_c_source {
	foreach $file (glob("$gda_base/share/gda/templates/*-srv*.tmpl")) {
		chomp($file);
		my $outfile = $file;
		$outfile =~ s/.*\///g;
		$outfile =~ s/-srv(.*).tmpl/-$provider\1/;
		
		open(INFILE, "<$file");
		open(OUTFILE, ">$outfile");
	
		while(<INFILE>) {
			my $line = $_;
			chomp($line);
			$line =~ s/<provider-name>/$provider/g;
			$line =~ s/<PROVIDER-NAME>/$provider_uc/g;
			$line =~ s/<your name>/$name/g;
			$line =~ s/<your-name>/$name/g;
			$line =~ s/<uuid>/$uuid/g;
			printf(OUTFILE "%s\n", $line);
		}

		close(OUTFILE);
		close(INFILE);
	}
}

sub create_oaf_info {
	open(OAFINFO, ">gda-$provider.oafinfo");
	print OAFINFO <<OAFINFO_EOF;
<oaf_info>

<oaf_server iid="OAFIID:gda-$provider:$uuid"
           type="exe"
       location="gda-$provider-srv">
        <oaf_attribute name="repo_ids" type="stringv">
                <item value="IDL:GDA/ConnectionFactory:1.0"/>
        </oaf_attribute>
        <oaf_attribute name="description" type="string"
                      value="GDA Datasource Access for $provider"/>
        <oaf_attribute name="gda_params" type="stringv">
                <item value="DATABASE"/>
        </oaf_attribute>
</oaf_server>

</oaf_info>
OAFINFO_EOF
	close(OAFINFO);
}

sub create_makefile_am {
	open(MAKEFILEAM, ">Makefile.am");
	print MAKEFILEAM <<EO_MAKEFILEAM;
bin_PROGRAMS = gda-$provider-srv
lib_LTLIBRARIES = libgda-$provider.la

INCLUDES = \\
	-I. \\
	\$(GDA_PROVIDER_CFLAGS) \\
	-I\$(top_builddir)/lib/gda-common \\
	-I\$(top_builddir)/lib/gda-server \\
	\$($provider_uc\_CFLAGS)

gda_$provider\_srv_SOURCES = main-$provider.c
gda_$provider\_srv_LDADD = \\
	\$(GDA_PROVIDER_LIBS) \\
	\$(top_builddir)/lib/gda-common/libgda-common.la \\
	\$(top_builddir)/lib/gda-server/libgda-server.la \\
	\$(top_builddir)/providers/gda-$provider-server/libgda-$provider.la \\
	\$($provider_uc\_LIBS)

EO_MAKEFILEAM
   printf(MAKEFILEAM "libgda_%s_la_SOURCES =", $provider);
	foreach $source (@TEMPLATES) {
		$source =~ s/.*\///g;
		$source =~ s/-srv(.*).tmpl/-$provider\1/;
		if ((($source =~ m|\.c$|) || ($source =~ m|\.h$|)) &&
		    ($source !~ m|main-$provider.c|)) {
			print MAKEFILEAM " \\ \n";
			printf(MAKEFILEAM "\t%s", $source);
		}
	}
	printf(MAKEFILEAM "\n");
	print MAKEFILEAM <<EOF;

EXTRA_DIST = gda-$provider.oafinfo

oafinfodir=\$(GDA_oafinfodir)
oafinfo_DATA=gda-$provider.oafinfo
EOF
	close(MAKEFILEAM);
}

sub create_misc {
	open(CVSIGNORE, ">.cvsignore");
	print CVSIGNORE <<EO_CVSIGNORE;
Makefile
Makefile.in
*.o
*.lo
libgda-$provider
gda-$provider-srv
.deps
.libs
EO_CVSIGNORE
	close(CVSIGNORE);
}

sub gen_uuid {
	open(UIDH, "uuidgen|");
	my $uuid = <UIDH>;
	close(UIDH);
	chomp($uuid);
	return "$uuid";
}
