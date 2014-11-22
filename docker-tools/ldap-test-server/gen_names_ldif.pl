#!/usr/bin/perl

use strict;
use warnings;

my $filename = 'names.csv';
my $base="ou=people,dc=gnome-db,dc=org";
my $mailsuffix = "gnome-db.org";

my $usersfile = "ldif-data/users.ldif";
my $groupsfile = "ldif-data/users-groups.ldif";

unless(open USERS, '>'.$usersfile) {
	# Die with error message 
	# if we can't open it.
	die "Unable to create $usersfile";
}
unless(open GROUPS, '>'.$groupsfile) {
	# Die with error message 
	# if we can't open it.
	die "Unable to create $groupsfile";
}

if (open(my $fh, '<:encoding(UTF-8)', $filename)) {
    while (my $line = <$fh>) {
	chomp $line;

	if ($line =~ /^#/) {
	    # ignore: comment
	    next;
	}

	my @lparts = split (/\//, $line);
	my $name = $lparts[0];
	my $photo = $lparts[1] ? $lparts[1] : "";

	#
	# name and photo
        #
	my $udn = "cn=$name,$base";
	print USERS "dn: $udn\n";
	print USERS "objectClass: inetOrgPerson\n";

	my @parts = split (/ /, $name, -1);
	my $p1 = $parts [0];
	my $p2 = $parts [1];
	my $lc1 = lc ($p1);
	my $lc2 = lc ($p2);
	my $uid = "$lc1.$lc2";
	print USERS "uid: $uid\n";
	print USERS "sn: $p2\n";
	print USERS "givenName: $p1\n";
	print USERS "cn: $name\n";
	print USERS "displayName: $name\n";
	my $pass = "$p1$p2";
	$pass =~ s/a/@/g;
	$pass =~ s/i/1/g;
	print USERS "userPassword: $pass\n";
	my $mail = "$uid\@$mailsuffix";
	print USERS "mail: $mail\n";
	print USERS "description: Call me $p1\n";
	if ($photo ne "") {
	    print USERS "jpegPhoto:< file:///ldif-data/$photo\n";
	}
	print USERS "\n";

	#
	# groups
        #
	my $groups = $lparts[2] ? $lparts[2] : "";
	#print "[$groups]\n";
	if ($groups ne "") {
	    my @gsplit = split (/\+/, $groups);
	    foreach (@gsplit) {
		my $gdn = "$_,ou=groups,dc=gnome-db,dc=org";
		print GROUPS "dn: $gdn\n";
		print GROUPS "changetype: modify\n";
		print GROUPS "add: uniqueMember\n";
		print GROUPS "uniqueMember: $udn\n";
		print GROUPS "\n";
	    }
	}
    }
} else {
    die "Could not open file '$filename'";
}

close USERS;
close GROUPS;
