echo "Running Upgrade..."
yum -y -q upgrade
echo "Installing packages..."
yum -y install postgresql-server postgresql postgresql-contrib supervisor psmisc
echo "Cleaning..."
yum clean all

mv /setup-data/supervisord.conf /etc/supervisord.conf

echo "Server setup"
/setup-data/postgresql-setup initdb

mv /setup-data/postgresql.conf /var/lib/pgsql/data/postgresql.conf
chown -v postgres.postgres /var/lib/pgsql/data/postgresql.conf

echo "host    all             all             0.0.0.0/0               md5" >> /var/lib/pgsql/data/pg_hba.conf

/setup-data/setup-data.sh
