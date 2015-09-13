echo "Running Upgrade..."
yum -y -q upgrade
echo "Installing packages..."
yum -y install nginx php-pdo php-pear php-fpm php-pgsql php-mysql psmisc tar net-tools
echo "Cleaning..."
yum clean all

cp /setup-data/nginx.conf /etc/nginx/
cp /setup-data/index.html /usr/share/nginx/html/
cp /setup-data/phpinfo.php /usr/share/nginx/html/
pushd /usr/share/nginx/html && tar xf /setup-data/php.tar && mv providers/web/php . && rm -rf providers && popd

pushd /usr/share/nginx/html/php/
mv gda-config.php gda-config.php.orig && cat gda-config.php.orig | sed -e 's/\/\/set_include_path/set_include_path/' > gda-config.php
mv gda-tester.php gda-tester.php.orig && cat gda-tester.php.orig | sed -e 's/test_connections = false/test_connections = true/' > gda-tester.php

mv /setup-data/gda-secure-config.php.tmpl .
popd
