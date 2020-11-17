# Onramp

Onramp constructs transaction aligned augmented diffs that provide immediately consistent full history augmented diffs sequenced by OSM transactions(?) rather than timestamps. Contrast this with Overpass API, which provides eventually consistent, time aligned augmented diffs that do not take notice of intermediate changes within the requested range (deltas).

If the above description was difficult to unpack or you're unfamiliar with OSM terminology, read on below for a more complete explanation.

## What is an Augmented Diff

Augmented diffs offer "a comparison of the result of the same [OSM] query at two different times". An augmented diff contains all nodes, ways, and relations that differ between the chosen start and end time written to a file in XML format using a consistent schema.

### The Schema

For the purposes of this document, it is important to note here that an augmented diff is an XML file containing:

- An indication as to whether each element was added, changed, or deleted
- The representation of each OSM object at the start time
- The representation of each OSM object at the end time
- The geometry of each OSM object at start and end time
- Any metadata associated with each OSM object at start and end time

A more comprehensive review of the schema of an augmented diff XML file and its contents is available [here](https://wiki.openstreetmap.org/wiki/Overpass_API/Augmented_Diffs).

In addition to the above, note that Overpass API augmented diffs to not report any intermediate state encountered between the start and end time. If object A at start time is updated to object B at time less than end but greater than start, and then object B is update to object C before end time, only states A and C will be visible in the augmented diff.

Onramp conforms to the augmented diff schema used by Overpass and presented in the OSM Wiki.

#### Onramp Augmented Diff Metadata for Untagged Nodes

Onramp relies on [OSMExpress](https://github.com/protomaps/osmexpress) to reconstruct augmented diffs from OSM change files. OSMExpress does not store metadata other than version for nodes that do not have tags [[osmexpress#12]](https://github.com/protomaps/OSMExpress/issues/12), [[osmexpress#25]](https://github.com/protomaps/OSMExpress/issues/25). Since these untagged nodes typically form the points of way objects, not writing metadata for them was deemed an acceptable tradeoff to keep the OSMExpress database size down since the interesting metadata is on the parent way.

To improve compatibility with downstream services consuming augmented diffs, whenever we don't know a metadata value we write the default value [as defined by libosmium](https://github.com/osmcode/libosmium/blob/master/include/osmium/osm/object.hpp#L69-L74).

If you encounter a use case for augmented diffs where this limitation affects you, please open an Onramp issue.

### Changesets and Changeset Boundaries

**Changeset:** For the purposes of this section, we are referring to an "Osmosis changeset" as described in the glossary below, _not_ an "API changeset".

Osmosis defines two different ways to describe splitting OSM changes into changeset boundaries, "time aligned" and "transaction aligned", which are described [here in more detail](changeset_boundary_types.md). The takeaway here is that a time aligned augmented diff will only contain OSM objects with a `timestamp` bounded by the start and end time. A transaction aligned augmented diff will contain OSM objects that were committed to the OSM database between the start and end time and may contain objects with timestamps outside the start and end times.

The Overpass API generates augmented diffs based on time-aligned boundaries. This results in augmented diffs that are "eventually consistent" because OSM changes valid within the requested time range will not be applied to an augmented diff until the transaction containing them is committed. This could take seconds, minutes, hours or even days. In practice, this results in an Overpass augmented diff query changing over time as new changes are committed that are valid in the time range requested.

Onramp, on the other hand, generates transaction aligned augmented diffs. These augmented diffs are immediately consistent when generated, as an osm database plus a change file being applied to the database contains all the information necessary to construct an augmented diff for the transaction. By definition all data is already committed to the database.

So, while both Overpass and Onramp generate an XML augmented diff with the same compatible schema, the set of OSM objects in the diff for a given sequence id will not be consistent between the two tools. The correct changeset boundary type to choose depends on the downstream use case. See [Use Cases](##use_cases) for some examples.

### Full history versus delta changes

As previously stated, augmented diffs describe the difference between an object at a start and end time. What will be visible in the augmented diff in the following scenario?

> An OSM object with version 1 is valid before the diff start time. Then, after the requested start time, the object is modified to version 2 and then again to version 3 before the diff end time.

An alternative representation might look like this:

```
      <---- time ---->

v1 -> start -> v2 -> v3 -> end

```

In an augmented diff generated using delta changes, only version 1 and version 3 will be visible. Version 2 is "rolled up" because it is only valid within the range and only the state at exactly t = start and t = end is considered. This is the strategy that Overpass augmented diffs use.

In an augmented diff generated using full history changes, all three versions are visible in the augmented diff. This strategy is utilized by Onramp. In the future, Onramp might provide an option to switch change strategy -- if this feature would support your use case, [contact us](info@azavea.com).

Full history changes will increase augmented diff size by some factor, but it is unclear what that is in practice and if it is significant. For Onramp, this tradeoff seemed worth it in order to be able to reconstruct a full history by stepping forwards or backwards through consecutive diffs.

The [OSM Wiki](./osm_change_types.md) describes these change types in more detail.

## Use Cases

### Onramp

Applications that compute real-time statistics, make use of geometries as they're committed to OSM or desire to step through OSM full history would be well served by Onramp.

### Overpass

Applications that are able to work around the eventually consistent nature of Overpass or are looking to easily access changes by time slices would continue to be better served by Overpass.

## Glossary

- API changeset: A set of changes performed by a single user against the API. An OSM API changeset is not transactional and may be used to represent a set of actions performed over a period of time. Example: [78798937](https://www.openstreetmap.org/changeset/78798937)
- Osmosis changeset: A set of changes contained in a change file (.osc). An Osmosis changeset is often referred to as a "diff" and contains a complete set of changes occurring between two points in time.
- sequence id: A key that maps to a single unique minute of time. A unix epoch can be converted to a sequence id via the formula: `unix_epoch = sequence_id * 60 + 1347432900`.
