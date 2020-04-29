# OSM Change Types

This document is reproduced from the [OSM Wiki](https://wiki.openstreetmap.org/wiki/Osmosis/Replication#Full_History_Changes_versus_Delta_Changes) for completeness.

## Full History Changes versus Delta Changes

Historically, Osmosis replication provided a way to update a set of data from one point in time to another. In other words, it aimed to make it efficient to keep a local copy of the planet up to date. As a result of this, the changesets (often referred to as "diff" files) only contained the differences between two points in time.

As the idea of replication matured and it became a more integral part of the OSM landscape, it became desirable to not only describe the differences in data between two points in time, but to include all data that changed between two points in time. This may be best explained with an example.

Let's say that a node with id 100 was created on Monday. This action created version 1 of the node. If a planet file were produced at the end of Monday, it would include version 1 of node 100. Let's say that on Tuesday the node was modified twice creating version 2 and version 3 of the node. If a new planet were created at the end of Tuesday, it would include version 3 of node 100. If a old-style delta style changeset was produced for the Tuesday period it would contain only version 3 of node 100 allowing a planet produced at the end of Monday to be patched efficiently to become an end of Tuesday planet. On the other hand, if a full history changeset was produced for the Tuesday period, it would contain both version 2 and version 3 of node 100 allowing a consumer to determine all changes that had occurred on Tuesday.

Original changesets were always delta style changesets. Over time, all replication jobs are all moving to full history format changesets due to the additional flexibility they provide. The Osmosis --simplify-change task allows full history changesets to be collapsed into delta style changesets for those tasks that require delta style data. 
