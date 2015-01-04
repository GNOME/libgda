echo "Running Upgrade..."
yum -y -q upgrade
echo "Installing packages..."
yum -y install community-mysql-server
echo "Cleaning..."
yum clean all

echo "Server setup"
/setup-data/mysql-setup
/setup-data/setup-data.sh
