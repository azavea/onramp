# OSM OnRamp

Generates immediately consistent OSM augmented diffs from the OSM change stream without Overpass

## Cloning

This project uses git submodules. After cloning, initialize them with:
```
git submodule update --init --recursive
```

## Development Environment

Requires either:

  - Docker 19+ and Docker Compose 1.25+
  - Vagrant 2.2+

Currently, the only supported action is to run OSMExpress within the development environment.

On a 2017 MacBook Pro with 2.5 Ghz dual core laptop, generating an osmx from
[pennsylvania-latest.osm.pbf](http://download.geofabrik.de/north-america/us/pennsylvania-latest.osm.pbf)
takes:

| Strategy | Time |
|----------|------|
| Docker, bind mount | 76s  |
| VM, shared folder | 93s |
| VM, outside shared folder| 47s |

### Docker

Checkout OSMExpress to a separate folder then build the container there:

```bash
cd path/to/OSMExpress
docker build -t .
```

Then cd back to this directory to run osmx via docker-compose:

```bash
docker-compose run --rm osmx <cmd> <args>
```

The `./data` folder is mounted into the container at `/data`.

### Vagrant

Run `vagrant up`. Once the machine provisions, `osmx` will be available on the path anywhere in the VM.

A scratch directory `/data` is created automatically in the VM. It is recommended to copy your osm.pbf
files there before creating and querying osmx databases. This avoids I/O heavy operations within the
mounted shared folder.

Note that the default volume size of the VM is 50Gb. If you need more space you'll have to tweak the Vagrantfile.
