TCT-stub
========

Intel Tizen Compatibility Test Stub

to build ARM version locally(without OBS), please make sure 

1, install tizen2.0 SDK with all component selected, 

2, replace all "/home/danny/" in build_data to your own home path, such as "/home/wl/"

3, run native-make -t device in CommandLineBuild folder.


to build unit test, just run "make ut" in root folder of the project. then run valgrind to check memory leak.

valgrind --tool=memcheck --leak-check=yes --track-origins=yes -v ./ut