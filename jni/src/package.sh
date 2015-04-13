#! /bin/sh
dx=/home/bearclaw/Android/Sdk/build-tools/21.1.2/dx
aapt=/home/bearclaw/Android/Sdk/build-tools/21.1.2/aapt
set -e
javac $(find . -name '*.java')
rm -f Temp.jar
jar -cvf Temp.jar io Test.class Test.java
rm -f classes.dex
$dx --dex --output=classes.dex Temp.jar
rm -f CmdLine.jar
$aapt add CmdLine.jar classes.dex


