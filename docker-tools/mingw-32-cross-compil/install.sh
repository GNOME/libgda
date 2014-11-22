echo "Running Upgrade..."
yum -y -q upgrade
echo "Installing packages..."
yum -y install gcc make intltool itstool glib2-devel mingw32-gcc mingw32-nsis mingw32-gtk3 mingw32-libxml2 mingw32-postgresql mingw32-libsoup mingw32-goocanvas2 mingw32-gtksourceview3 mingw32-libxslt iso-codes-devel mingw32-readline zip
echo "Cleaning..."
yum clean all
