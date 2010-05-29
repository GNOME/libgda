#!/bin/sh

dmgfile=GdaBrowser.dmg
current=`pwd`
rm -f ${dmgfile}
cd ../CreateDMG
./create-dmg --window-size 500 300 --background ${current}/background.jpg --icon-size 96 --volname GdaBrowser --icon "Applications" 380 150 --icon GdaBrowser 120 150 ${current}/${dmgfile} ${current}/build/
