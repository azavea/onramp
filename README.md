# OSM OnRamp

Generates immediately consistent OSM augmented diffs from the OSM change stream without Overpass

## Cloning

This project uses git submodules. After cloning, initialize them with:

```
git submodule update --init --recursive
```

## Building

SSH into Vagrant VM with `vagrant ssh` then run `make`.

Use the generated `onramp` executable in the VM as desired.

## Development Environment

Requires:

- Vagrant 2.2+

Run `vagrant up`. Once the machine provisions, `osmx` will be available on the path anywhere in the VM.

A scratch directory `/data` is created automatically in the VM. It is recommended to copy your osm.pbf
files there before creating and querying osmx databases. This avoids I/O heavy operations within the
mounted shared folder.

Note that the default volume size of the VM is 50Gb. If you need more space you'll have to tweak the Vagrantfile.
