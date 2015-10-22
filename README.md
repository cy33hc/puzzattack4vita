## puzzattack4vita
This is simple tetris attack clone. Original source in from the following code google link https://code.google.com/p/puzzattack.

The keys
* LEFT,RIGHT,UP,DOWN -- for moving the cursor around the grid
* CROSS -- swap the 2 sqaure the cursor is on
* LTRIGGER/RTRIGGER -- scroll an new row of squares

## Compiling

In order two compile this sample you need to download:
* [vita-sdk] (https://github.com/vitasdk)
* [vita_portlibs] (https://github.com/xerpi/vita_portlibs.git)
* [vita2dlib] (https://github.com/xerpi/vita2dlib.git)
* [bin2c] (http://sourceforge.net/projects/bin2c/)

First you need to install VITASDK, on my OS I've installed it to:
```
	/home/rejuvenate/vitasdk
```
You can find the prebuilt sdk from following link:
* [Unofficial VitaSDK pre-built] (http://wololo.net/talk/viewtopic.php?f=111&t=44196)

```

Once you got it installed, first setup an environment variable called VITASDK, for example by adding to the ~/.bashrc:
```
	export VITASDK=/home/rejuvenate/vitasdk
	export PATH=$PATH:$VITASDK/bin
```

Then you need to compile and install vita_portlibs:
```
	$ cd vita_portlibs
	$ make zlib
	$ make install-zlib
	$ make
	$ make install
```

Now we need to install vita2dlib:
```
	$ cd vita2dlib
	$ libvita2d
	$ make
	$ make install
```

Now you can compile this homebrew by typing:
```
	$ build.sh

	or

	$ sh build.sh
```

The resulting file will be called: puzzattack4vita.velf
