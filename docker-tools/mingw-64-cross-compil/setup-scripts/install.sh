echo "Running Upgrade..."
yum -y -q upgrade
echo "Installing packages..."
yum -y install gcc make intltool itstool glib2-devel mingw64-gcc mingw64-nsis mingw64-gtk3 mingw64-libxml2 mingw64-postgresql mingw64-libsoup mingw64-goocanvas2 mingw64-gtksourceview3 mingw64-libxslt iso-codes-devel mingw64-readline zip
echo "Cleaning..."
yum clean all
