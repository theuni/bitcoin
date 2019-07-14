# Mac Deployment

The `macdeployqtplus` script should not be run manually. Instead, after building as usual:

```bash
make deploy
```

During the deployment process, the disk image window will pop up briefly 
when the fancy settings are applied. This is normal, please do not interfere,
the process will unmount the DMG and cleanup before finishing.

When complete, it will have produced `Bitcoin-Core.dmg`.

## SDK Extraction

The `extract-osx-sdk.sh` script in this directory can be used to extract 
our previously used macOS SDK, from the `Xcode_7.3.1.dmg`.

After version 7.x, Apple started shipping the `Xcode.app` in a `.xip` archive.
This makes the SDK less-trivial to extract on non-macOS machines.
One approach (tested on Debian Buster) is outlined below:

```bash

apt install clang cpio git liblzma-dev libxml2-dev libssl-dev make

git clone https://github.com/tpoechtrager/xar
pushd xar/xar
./configure
make
make install
popd

git clone https://github.com/NiklasRosenstein/pbzx
pushd pbzx
clang -llzma -lxar pbzx.c -o pbzx -Wl,-rpath=/usr/local/lib
popd

xar -xf Xcode_10.2.1.xip -C .

./pbzx/pbzx -n Content | cpio -i

find Xcode.app -type d -name MacOSX.sdk -execdir sh -c 'tar -c MacOSX.sdk/ | gzip -9n > /MacOSX10.14.sdk.tar.gz' \;
```
