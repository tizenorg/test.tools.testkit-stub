TCT-stub
========

Intel Tizen Compatibility Test Stub

###to build and install x86 version on linux pc,###
1, make

2, sudo make install



###to build ARM version locally(without OBS), please make sure ###

1, install tizen2.0 SDK with all component selected, 

2, replace all "/home/danny/" in file "CommandLineBuild/build_data" with your own home path, such as "/home/test/"

3, run native-make -t device in CommandLineBuild folder.


to build unit test, just run "make ut" in root folder of the project. then run valgrind to check memory leak.

valgrind --tool=memcheck --leak-check=yes --track-origins=yes -v ./ut

###to build on windows###
1, use git bash to maintain code

2, use msys and mingw to compile

3, to build the target, run "make win"

###for Android###
1, for x86 device, run "make android"

2, for ARM or mips, install Android NDK at first, then run ndk-build in jni folder

3, use eclipse+ADT to import android project from TCT-stub folder, build apk file for wrapper app of stub in android target
