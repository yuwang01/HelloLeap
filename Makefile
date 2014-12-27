OS := $(shell uname)
ARCH := $(shell uname -m)
FRAMEWORKS = -framework Cocoa -framework OpenGL -framework IOKit -framework CoreVideo
GLLIBS = -lglfw3 -lglew 
LEAPDIR = /Path/to/LeapMotion/LeapDeveloperKit_2.2.0+23475_mac/LeapSDK
LEAPLIB = -I$(LEAPDIR)/include $(LEAP_LIBRARY) -headerpad_max_install_names
ifeq ($(OS), Linux)
  ifeq ($(ARCH), x86_64)
    LEAP_LIBRARY := $(LEAPDIR)/lib/x64/libLeap.so -Wl,-rpath,../lib/x64
  else
    LEAP_LIBRARY := $(LEAPDIR)/lib/x86/libLeap.so -Wl,-rpath,../lib/x86
  endif
else
  # OS X
  LEAP_LIBRARY := $(LEAPDIR)/lib/libLeap.dylib
endif

HandWireframe: HandWireframe.o maths_funcs.o
	g++ -Wall -g $(LEAPLIB) HandWireframe.o maths_funcs.o -o HandWireframe $(GLLIBS) $(FRAMEWORKS)
ifeq ($(OS), Darwin)
	install_name_tool -change @loader_path/libLeap.dylib $(LEAPDIR)/lib/libLeap.dylib HandWireframe
endif
	rm *.o;

HandWireframe.o: HandWireframe.cpp
	g++ -Wall -g -I$(LEAPDIR)/include HandWireframe.cpp -c -o HandWireframe.o

maths_funcs.o: maths_funcs.cpp
	g++ -Wall -g maths_funcs.cpp -c -o maths_funcs.o

clean:
	rm -rf HandWireframe.dSYM
	rm HandWireframe
	rm GL.log
