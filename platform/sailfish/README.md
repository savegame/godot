## install SailfishSDK 
1. Download and install [SailfishSDK](https://sailfishos.org/wiki/Application_SDK)
2. (optional) Install docker image, use [this instcruction](https://github.com/CODeRUS/docker-sailfishos-sdk-local) from @CODeRUS

## Add SailfishSDK/bin folder to PATH for using sfdk
```shell
export PATH=$HOME/SailfishSDK/bin:${PATH} 
```
now you can use **sfdk** tool  
```shell
sfdk engine exec <your command inside buildengine> 
```
or, you can use absolute path to **sfdk** 
```shell
~/SailfishOS/bin/sfdk engine exec echo "Hello from SailfishOS SDK build engine"
```

### Install dependencies in SailfishSDK targets
check list of targets
```shell
sfdk engine exec sb2-config -l
```

should output something like this 

```shell
SailfishOS-4.3.0.12-aarch64.default
SailfishOS-4.3.0.12-aarch64
SailfishOS-4.3.0.12-armv7hl.default
SailfishOS-4.3.0.12-armv7hl
SailfishOS-4.3.0.12-i486.default
SailfishOS-4.3.0.12-i486
```

**armv7hl**
```sh
sfdk engine exec sb2 -t SailfishOS-4.3.0.12-armv7hl -R zypper in -y SDL2-devel systemd-devel libaudioresource-devel pulseaudio-devel openssl-devel libwebp-devel libvpx-devel wayland-devel libpng-devel scons
```

**i486**
```sh
sfdk engine exec sb2 -t SailfishOS-4.3.0.12-i486 -R zypper in -y SDL2-devel systemd-devel libaudioresource-devel pulseaudio-devel openssl-devel libwebp-devel libvpx-devel wayland-devel libpng-devel scons
```

**aarch64**
```sh
sfdk engine exec sb2 -t SailfishOS-4.3.0.12-aarch64 -R zypper in -y SDL2-devel systemd-devel libaudioresource-devel pulseaudio-devel openssl-devel libwebp-devel libvpx-devel wayland-devel libpng-devel scons
```

or by **one line** script for all targets in same time ;) :
```shell
for each in `sfdk engine exec sb2-config -l|grep -v default`; do sfdk engine exec sb2 -t $each -R zypper in -y SDL2-devel libaudioresource-devel pulseaudio-devel openssl-devel libwebp-devel libvpx-devel wayland-devel libpng-devel systemd-devel scons; done
```

## Build Godot export template for Sailfish OS
Use sfdk from SailfishSDK/bin/sfdk 

```sh
export PATH=$HOME/SailfishSDK/bin:${PATH}
# building for armv7hl platfrom 
sfdk engine exec sb2 -t SailfishOS-4.3.0.12-armv7hl scons -j`nproc` arch=arm platform=sailfish tools=no bits=32 target=release  
# than do same for aarch64 platfrom
sfdk engine exec sb2 -t SailfishOS-4.3.0.12-aarch64 scons -j`nproc` arch=arm64 platform=sailfish tools=no bits=64 target=release 
# than do same for i486 platfrom
sfdk engine exec sb2 -t SailfishOS-4.3.0.12-i486    scons -j`nproc` arch=x86 platform=sailfish tools=no bits=32 target=release
# then make export tempalte smaller
sfdk engine exec sb2 -t SailfishOS-4.3.0.12-arm7hl  strip bin/godot.sailfish.opt.arm
sfdk engine exec sb2 -t SailfishOS-4.3.0.12-aarch64 strip bin/godot.sailfish.opt.arm64
sfdk engine exec sb2 -t SailfishOS-4.3.0.12-i486    strip bin/godot.sailfish.opt.x86
```

## Build Godot Editor with SailfishOS export menu
### build on Linux  X11
```shell
scons -j`nproc` arch=x64 platform=x11 tools=yes bits=64 traget=release_debug 
strip bin/godot.x11.tools.x64
```
### build for Win64 with cross compile toolkit on Linux
First, install cross compile gcc
#### on ubuntu 
```
sudo apt install -y mingw-w64
```
then choose posix compiler, with commands below
```
sudo update-alternatives --config x86_64-w64-mingw32-gcc 
sudo update-alternatives --config x86_64-w64-mingw32-g++
```
#### on fedora (centos)
```
sudo dnf install -y  mingw64-gcc-c++ mingw64-gcc
```
#### build an editor 
```shell
#ubuntu 
CXX=x86_64-w64-mingw32-g++ CC=x86_64-w64-mingw32-gcc scons -j`nproc` arch=x64 platform=windows tools=yes bits=64 traget=release_debug 
x86_64-w64-mingw32-strip bin/godot.windows.tools.x64.exe
```