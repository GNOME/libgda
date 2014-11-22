echo "Running Upgrade..."
yum -y -q upgrade
echo "Installing packages..."
yum -y install openldap-servers openldap-clients psmisc
echo "Cleaning..."
yum clean all

echo "LDAP setup"
/usr/sbin/slapd -h "ldap:/// ldapi:///" -u ldap

echo "LDAP configuration"
mkdir -p /var/lib/ldap/gnome-db
chown ldap /var/lib/ldap/gnome-db

ldapadd -Y EXTERNAL -H ldapi:// -f /etc/openldap/schema/cosine.ldif
ldapadd -Y EXTERNAL -H ldapi:// -f /etc/openldap/schema/inetorgperson.ldif
ldapadd -Y EXTERNAL -H ldapi:// -f /etc/openldap/schema/nis.ldif

ldapadd -Y EXTERNAL -H ldapi:/// -f /ldif-data/setup.ldif
ldapadd -x -D cn=admin,dc=gnome-db,dc=org -w TnUtfv0NFI -f /ldif-data/orga.ldif
ldapmodify -Y EXTERNAL -H ldapi:/// -f /ldif-data/set-rights.ldif
ldapadd -x -D cn=ldapadmin,dc=gnome-db,dc=org -w wmMF3fd2FW -f /ldif-data/users.ldif
ldapmodify -x -D cn=ldapadmin,dc=gnome-db,dc=org -w wmMF3fd2FW -f /ldif-data/users-groups.ldif
ldapadd -x -D cn=admin,dc=gnome-db,dc=org -w TnUtfv0NFI -f /ldif-data/orga-clean.ldif

echo "LDAP shutdown"
killall slapd

