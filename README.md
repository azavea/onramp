# Onramp

Onramp provides infrastructure for generating transaction-aligned OpenStreetMap [augmented diffs](https://wiki.openstreetmap.org/wiki/Overpass_API/Augmented_Diffs) using [OSMExpress](https://github.com/protomaps/OSMExpress).

Note that the transaction-aligned augmented diffs will contain slightly different results when compared against Overpass API time-aligned augmented diffs for the same minute. For more details, see the [Onramp documentation](./docs/index.md).

## Prerequisites

- Docker
- Docker Compose 18+

You'll need an OSMExpress database. To build one, download any extract from https://download.geofabrik.de/ to `./data`.

Expand the extract into an osmx database with: `docker-compose run --rm osmx expand /data/<filename>.osm.pbf /data/<filename>.osmx`.

## Usage

### osmx

Any OSMExpress command available here can be run locally with `./data` mounted to `/data` in the `osmx` container.

For a complete list of options run `osmx` without options:

```shell
docker-compose run --rm osmx
```

### osmx-update

The osmx database can be updated with the `./app/osmx-update` script by running:

```shell
docker-compose run --rm --entrypoint="python3 ./osmx-update" onramp \
  /data/<path-to-db>.osmx \
  https://hostname/path/to/replication/ \
```

For example, if you've generated an osmx database from the geofabrik USA Pennsylvania extract, the command would be:

```shell
docker-compose run --rm --entrypoint="python3 ./osmx-update" onramp \
  /data/pennsylvania.osmx \
  https://download.geofabrik.de/north-america/us/pennsylvania-updates/
```

### augmented-diff.py

Query the osmx database for the current sequence number:

```shell
docker-compose run --rm osmx query /data/database.osmx
```

Download the _next_ sequence number from the replication server:

```
> docker-compose run --rm osmx query /data/pennsylvania.osmx
locations: 21278878
nodes: 434260
ways: 2261628
relations: 32178
cell_node: 21278878
node_way: 23499628
node_relation: 30379
way_relation: 401824
relation_relation: 1479
Timestamp: 2020-07-29T20:56:03Z
Sequence #: 2690  # <-- This is the number required
> curl "https://download.geofabrik.de/north-america/us/pennsylvania-updates/000/002/691.osc.gz" | gunzip -c > ./data/pa-2691.osc
```

An augmented diff can be generated with the `./app/augmented_diff.py` script by running:

```shell
docker-compose run --rm --entrypoint="python3 ./augmented_diff.py" onramp \
  /data/pennsylvania.osmx \
  /data/pa-2691.osc \
  /data/pa-2691.adiff.xml
```

## License

Copyright Azavea
Apache2, see [LICENSE](./LICENSE)
