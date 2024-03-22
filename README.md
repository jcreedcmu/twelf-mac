MacOS emulation & dev notes
===========================

Thanks a million to [agoode](https://github.com/agoode) for explaining a lot of this.

I'm using [Retro68](https://github.com/autc04/Retro68) to compile C to run on an emulated mac. I build qemu 8.2.2 from source with
```
sudo apt install libglib2.0-dev libpixman-1-dev libgtk-3-dev libasound2-dev libslirp-dev
mkdir build
cd build
../configure --target-list=m68k-softmmu --enable-gtk  --enable-pixman --enable-slirp
make
```

I needed to put in this directory:

- `MacOS7.5.3.img`
  created by `qemu-img create -f raw -o size=2G MacOS7.5.3.img`
- `MacOS7.5.3.iso`
  from https://winworldpc.com/download/3dc3aec3-b125-18c3-9a11-c3a4e284a2ef
- `pram-macos.img`
  created by `qemu-img create -f raw pram-macos.img 256b`
- `Quadra800.rom`
  from https://archive.org/details/mac_rom_archive_-_as_of_8-19-2011 and
  `mv 'F1ACAD13 - Quadra 610,650,maybe 800.ROM' Quadra800.rom`
- `Saved.hda`
  created by going to https://infinitemac.org/1996/System%207.5.3 and moving
  some files (like netscape navigator and stuffit expander) to `Saved HD`
  and then mousing over the grey "Home" next to the apple icon, choosing "Settings"
  and doing "Save Disk Image" as `.hda`

Building and Running the App
----------------------------

Currently starting with the `Dialog` example from https://github.com/autc04/Retro68
```shell
cd docker
make app.bin
make serve # spawns a local web server
```

Start MacOS guest with `./qemu-macos`. Inside MacOS guest:
- Launch Netscape Navigator from `Saved HD`.
- Go to `http://10.0.2.2:8000/` (this is the host-local web server)
- Download `app.bin`
- Expand it in Stuffit Expander to create app named `Dialog`.