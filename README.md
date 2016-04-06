##Android Host Knife

#### How to build?

	cd android-host-knife/
	make -f Makefile.$(your-platform) CROSS_COMPILE=$(if-you-need)
	find what you build in ./output/bin/

##### How to use
	cp all ./output/bin/* to your PATH
	
	or you can use my prebuild
	
	on Linux:
	source ./setenv.sh

	on Windows:
	.\setenv.bat