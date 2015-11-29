#!/bin/bash
version=11.2.0
debfile=/setup-data/oracle-xe_${version}-1.0_amd64.deb

# avoid dpkg frontend dialog / frontend warnings 
export DEBIAN_FRONTEND=noninteractive

# Prepare to install Oracle
apt-get update && apt-get install -y libaio1 net-tools bc rlwrap
ln -s /usr/bin/awk /bin/awk
mkdir /var/lock/subsys
mv /setup-data/chkconfig /sbin/chkconfig
chmod 755 /sbin/chkconfig

# Install Oracle
dpkg --install $debfile && rm $debfile

# Backup listener.ora as template
cp /u01/app/oracle/product/$version/xe/network/admin/listener.ora /u01/app/oracle/product/$version/xe/network/admin/listener.ora.tmpl

mv /setup-data/init.ora /u01/app/oracle/product/$version/xe/config/scripts
mv /setup-data/init2.ora ./u01/app/oracle/product/11.2.0/xe/dbs/init.ora
mv /setup-data/initXETemp.ora /u01/app/oracle/product/$version/xe/config/scripts

printf 8080\\n1521\\ngdauser\\ngdauser\\ny\\n | /etc/init.d/oracle-xe configure

echo "export ORACLE_HOME=/u01/app/oracle/product/$version/xe" >> /etc/bash.bashrc
echo 'export PATH=$ORACLE_HOME/bin:$PATH' >> /etc/bash.bashrc
echo "export ORACLE_SID=XE" >> /etc/bash.bashrc

echo "export version=$version" > /setup-data/env.sh
echo "export ORACLE_HOME=/u01/app/oracle/product/$version/xe" >> /setup-data/env.sh
echo 'export PATH=$ORACLE_HOME/bin:$PATH' >> /setup-data/env.sh

/setup.sh
