# Build the engine and export templates for AuroraOS with Mono support

First of all, download the source code of the engine to the Aurora working directory and assemble the engine and editor from the source code. This instruction used the Godot 3.6 version ported by sashikknox and slightly modified by me for use with mono.

## Installing dependencies on the desktop

**[Official instructions and list of dependencies](https://docs .godotengine.org/ru/3.x/development/compiling/index.html).**

The official documentation also describes in more detail how to build the engine for the main Desktop platforms with Mono. Below is a short instruction for Linux

## Downloading the engine sources and assembling them

My path for executing the `git clone` command is: `~/Aurora/src/`

```
git clone https://gitflic.ru/project/folgore/godot.git
cd godot
scons p=x11 tools=yes module_mono_enabled=yes mono_glue=no
bin/godot.x11.tools.64.mono --generate-mono-glue modules/mono/glue
scons p=x11 target=release_debug tools=yes module_mono_enabled=yes
strip bin/godot.x11.opt.tools.64.mono
```

**Be sure to make a backup copy of the virtual machine before further manipulations, if you doubt your actions and my instructions.**

## Installing dependencies on the Aurora build engine

P.S. The build was made under AuroraOS-4.0.2.303, so the targets in the instructions are `AuroraOS-4.0.2.303-base-i486` and `AuroraOS-4.0.2.303-base-armv7hl`, use your names for other targets.

### **i486**

Preparing the build engine for compiling godot export templates and working with mono on the emulator
We launch the terminal from the Aurora working directory. I have this `~/Aurora/`

```
sfdk engine exec sb2 -t AuroraOS-4.0.2.303-base-i486 -m sdk-install -R zypper --non-interactive install git autoconf libtool make gettext zlib gcc gdb gcc-c++ openssl-devel bzip2-devel xz-devel nano scons mesa-llvmpipe-libGLESv1-devel alsa-lib-devel pulseaudio-devel SDL2-devel SDL2_image-devel SDL2_net-devel SDL2_sound-devel SDL2_ttf-devel wayland-devel wayland-egl-devel wayland-protocols-devel openssl
```

### Mono Build

Download the mono sources to the Aurora working directory from the official website or from github (I tested the latest version from the Mono website **[12/6/11999](https://download.mono-project.com/sources/mono/mono-6.12.0.199.tar.xz ))**
*For an example. My path to the source archive is: `~/Aurora/src/mono/`*
``
tar -xvf ~/Aurora/src/mono/mono-12/6/11999.tar.xz -C ~/Aurora/src/mono/
``
Open the console from the mono source folder and go into the build engine and the emulator target

```
sfdk engine exec
sb2 -t AuroraOS-4.0.2.303-base-i486 -m sdk-install -R
```

Mono is built for the emulator and x86 devices using the following commands

```
./configure --prefix=/home/mersdk/mono_x86
make
make install
make clean
```
*Specify the prefix for installation in a specific folder. Since Mono is assembled in this manual for all SDK 4.02.303 targets, it is necessary that different files for different architectures get along with each other*

Creating a script for exporting global parameters:

```
nano ~/.mono_x86
```

Script Content:

```
#!/bin/bash

#Path to prefix with mono for i486
MONO_PREFIX=/home/mersdk/mono_x86

export DYLD_FALLBACK_LIBRARY_PATH=$MONO_PREFIX/lib:$DYLD_LIBRARY_FALLBACK_PATH
export LD_LIBRARY_PATH=$MONO_PREFIX/lib:$LD_LIBRARY_PATH
export C_INCLUDE_PATH=$MONO_PREFIX/include
export ACLOCAL_PATH=$MONO_PREFIX/share/aclocal
export PKG_CONFIG_PATH=$MONO_PREFIX/lib/pkgconfig
export PATH=$MONO_PREFIX/bin:$PATH
```

Exporting variables

```
source ~/.mono_x86
```

We are synchronizing certificates so that nuget can be used in future projects, for example.

```
cert-sync /etc/ssl/certs/ca-bundle.crt
```

We write the exit command twice to exit the target console and the build engine.

```
exit
exit
```

Go to the Godot source folder to compile the export templates.

```
cd ~/Aurora/src/godot
```

*If it is necessary to encrypt .pck scripts, then it is necessary to act according to [instructions](https://docs .godotengine.org/ru/3.x/development/compiling/compiling_with_script_encryption_key.html ) posted on the Godot website*

In the Godot source folder, run the commands to build the release and debug template (the second one, if necessary)

### Building a release export template

```
sfdk engine exec sb2 -t AuroraOS-4.0.2.303-base-i486 scons -j`nproc` arch=x86 platform=auroraos tools=no bits=32 target=release module_mono_enabled=yes mono_prefix="/home/mersdk/mono_x86/" mono_static=yes
```

### Building the debug export template

```
sfdk engine exec sb2 -t AuroraOS-4.0.2.303-base-i486 scons -j`nproc` arch=x86 platform=auroraos tools=no bits=32 target=debug module_mono_enabled=yes mono_prefix="/home/mersdk/mono_x86/" mono_static=yes
```

`module_mono_enabled=yes` activates the ability to use mono and the C# language to write scripts.
We pay special attention to `mono_prefix`, since the path must match the previously specified path when compiling Mono.
`mono_static=yes` is required, otherwise libraries such as `libmonosgen-2.0.so ` it will be necessary to somehow upload the project to the device, bypassing the validator.

After compilation, the file `godot.auroraos.opt.x86.mono` and the folder `data.mono.auroraos` will appear in the `bin' folder located in the downloaded Godot sources.32.release` (if you specified `target=debug`, then the file and folder will have the corresponding entry in the name).
We specify this file in the export settings when exporting the project. If you move the templates, be sure to copy the corresponding folder (indicated above), it contains the necessary libraries, which will be included in the project when exporting.

### Feature of Mono compilation under ARM
Installing the necessary dependencies for armv7hl

```
sfdk engine exec sb2 -t AuroraOS-4.0.2.303-base-armv7hl -m sdk-install -R zypper --non-interactive install git autoconf libtool make gettext zlib gcc gdb gcc-c++ openssl-devel bzip2-devel xz-devel nano scons mesa-llvmpipe-libGLESv1-devel alsa-lib-devel pulseaudio-devel SDL2-devel SDL2_image-devel SDL2_net-devel SDL2_sound-devel SDL2_ttf-devel wayland-devel wayland-egl-devel wayland-protocols-devel openssl
```
Go to the mono source folder

```
cd ~/Aurora/src/mono/mono-6.12.0.199
```
Launching the assembly engine terminal from the catalog

```
sfdk engine exec
sb2 -t AuroraOS-4.0.2.303-base-armv7hl
``
### Mono Build
``
./configure --prefix=/home/mersdk/mono_arm --disable-mcs-build
make
make install
make clean
```
Here we have specified a different path for the prefix, different from the previous one for the emulator and x86, and we also make sure to write `--disable-mcs-build`

Creating a script for exporting global parameters

```
nano ~/.mono_arm
```

File contents:
``
#!/bin/bash

#Path to prefix with mono for arm
MONO_PREFIX=/home/mersdk/mono_arm

export DYLD_FALLBACK_LIBRARY_PATH=$MONO_PREFIX/lib:$DYLD_LIBRARY_FALLBACK_PATH
export LD_LIBRARY_PATH=$MONO_PREFIX/lib:$LD_LIBRARY_PATH
export C_INCLUDE_PATH=$MONO_PREFIX/include
export ACLOCAL_PATH=$MONO_PREFIX/share/aclocal
export PKG_CONFIG_PATH=$MONO_PREFIX/lib/pkgconfig
export PATH=$MONO_PREFIX/bin:$PATH
```

**Before compiling the sources for arm, I recommend turning off the assembly engine, and running it again and exporting global variables for this platform, so that the mono_x86 and mono_arm variables do not mix.**

```
sfdk engine exec
sb2 -t AuroraOS-4.0.2.303-base-armv7hl -m sdk-install -R
source /home/mersdk/.mono_arm
exit
exit
```

*`/home/mersdk/` - the VM's home directory*

### Creating export templates

go to the Godot source folder

```
cd ~/Aurora/src/godot
```

### Build a release 32bit export template

``
sfdk engine exec sb2 -t AuroraOS-4.0.2.303-base-armv7hl scons -j'nproc` arch=arm platform=auroraos tools=no bits=32 target=release module_mono_enabled=yes mono_prefix="/home/mersdk/mono_arm/" mono_static=yes
``
### Export template build debug 32bit

```
sfdk engine exec sb2 -t AuroraOS-4.0.2.303-base-armv7hl scons -j`nproc` arch=arm platform=auroraos tools=no bits=32 target=release module_mono_enabled=yes mono_prefix="/home/mersdk/mono_arm/" mono_static=yes
```

*The export template for arm will take much longer to create…*

The other export templates are compiled in the same way.

Instructions for using export templates from Sashikknox to Godot can be found [here](https://boosty.to/sashikknox/posts/bad1c63b-c453-4933-a34c-ee7c22bd6e44 ?share=post_link)
