"""

From OSMExpress https://github.com/protomaps/osmexpress Copyright 2019 Protomaps.
All rights reserved. Licensed under 2-Clause BSD, see LICENSE

"""
from collections import namedtuple
import copy
from datetime import datetime
import logging
import sys

import osmx
import xml.etree.ElementTree as ET

from .geometry import Bounds
from .utils import indent
from .xml_writers import write_xml

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)
logger.addHandler(logging.StreamHandler(sys.stdout))


def augmented_diff(
    osmx_file,
    osc_file,
    output_file,
    end_timestamp=None,
    osc_sequence=None,
    osc_url=None,
):
    """ Generate an OSM Augmented Diff using osmx_file and osc_file

    end_timestamp is the timestamp of the end of the time range in the osc_file.

    Result written as xml to output_file, which can be a local file or S3 URI.

    See https://wiki.openstreetmap.org/wiki/Overpass_API/Augmented_Diffs
    This function should be called on an osmx_file that hasn't yet had osc_file
    written to it.

    """

    # 1st pass:
    # populate the collection of actions
    # create dictionary from osm_type/osm_id to action
    # e.g. node/12345 > Node()
    Action = namedtuple("Action", ["type", "element"])
    actions = {}

    osc = ET.parse(osc_file).getroot()
    for block in osc:
        for e in block:
            action_key = e.tag + "/" + e.get("id")
            # Always ensure we're updating to the latest version of an object for the diff
            if action_key in actions:
                newest_version = int(actions[action_key].element.get("version"))
                e_version = int(e.get("version"))
                if e_version < newest_version:
                    logger.warning(
                        "Element {}, version {} is less than version {}".format(
                            action_key, e_version, newest_version
                        )
                    )
                    continue
            actions[action_key] = Action(block.tag, e)

    action_list = [v for k, v in actions.items()]

    env = osmx.Environment(osmx_file)
    with osmx.Transaction(env) as txn:
        locations = osmx.Locations(txn)
        nodes = osmx.Nodes(txn)
        ways = osmx.Ways(txn)
        relations = osmx.Relations(txn)

        def not_in_db(elem):
            elem_id = int(elem.get("id"))
            if elem.tag == "node":
                return not locations.get(elem_id)
            elif elem.tag == "way":
                return not ways.get(elem_id)
            else:
                return not relations.get(elem_id)

        def get_lat_lon(ref, use_new):
            if use_new and ("node/" + ref in actions):
                node = actions["node/" + ref]
                lon = "{:.07f}".format(float(node.element.get("lon")))
                lat = "{:.07f}".format(float(node.element.get("lat")))
                return (lon, lat)
            else:
                ll = locations.get(ref)
                return ("{:.07f}".format(ll[1]), "{:.07f}".format(ll[0]))

        def rebuild_old_element(elem):
            elem_id = int(elem.get("id"))
            if elem.tag == "node":
                o = nodes.get(elem_id)
                ll = get_lat_lon(elem_id, False)
                elem.set("lon", ll[0])
                elem.set("lat", ll[1])
            elif elem.tag == "way":
                o = ways.get(elem_id)
                for n in o.nodes:
                    node = ET.SubElement(elem, "nd")
                    node.set("ref", str(n))
                it = iter(o.tags)
                for t in it:
                    tag = ET.SubElement(elem, "tag")
                    tag.set("k", t)
                    tag.set("v", next(it))
            else:
                o = relations.get(elem_id)
                for m in o.members:
                    member = ET.SubElement(elem, "member")
                    member.set("ref", str(m.ref))
                    member.set("role", m.role)
                    member.set("type", str(m.type))
                it = iter(o.tags)
                for t in it:
                    tag = ET.SubElement(elem, "tag")
                    tag.set("k", t)
                    tag.set("v", next(it))

            if o:
                elem.set("version", str(o.metadata.version))
                elem.set("user", str(o.metadata.user))
                elem.set("uid", str(o.metadata.uid))
                # convert to ISO8601 timestamp
                timestamp = o.metadata.timestamp
                formatted = datetime.utcfromtimestamp(timestamp).isoformat()
                elem.set("timestamp", formatted + "Z")
                elem.set("changeset", str(o.metadata.changeset))
            else:
                # tagless nodes
                try:
                    version = locations.get(elem_id)[2]
                except TypeError:
                    # If loc is None here, it typically means that a node was created and
                    # then deleted within the diff interval. In the future we should
                    # remove these operations from the diff entirely.
                    logger.warning(
                        "No old loc found for tagless node {}".format(elem_id)
                    )
                    version = "?"
                elem.set("version", str(version))
                elem.set("user", "?")
                elem.set("uid", "?")
                elem.set("timestamp", "?")
                elem.set("changeset", "?")

        # 2nd pass
        # create an XML tree of actions with old and new sub-elements
        o = ET.Element("osm")
        o.set("version", "0.6")
        o.set(
            "generator",
            "Overpass API not used, but achavi detects it at the start of string; https://github.com/azavea/onramp",
        )

        for action in action_list:
            a = ET.SubElement(o, "action")
            a.set("type", action.type)
            if action.type == "create":
                a.append(action.element)
            elif action.type == "delete":
                old = ET.SubElement(a, "old")
                new = ET.SubElement(a, "new")
                # get the old metadata
                modified = copy.deepcopy(action.element)
                rebuild_old_element(action.element)
                old.append(action.element)

                modified.set("visible", "false")
                for child in list(modified):
                    modified.remove(child)
                # TODO: The Geofabrik deleted elements seem to have the old metadata and
                # old version numbers. Check if this is true of planet replication files
                new.append(modified)
            elif not_in_db(action.element):
                # Typically occurs when:
                #  1. (TODO) An element is deleted but then restored later,
                #     which should remain a modify operation. This will be difficult
                #     because objects are not retained in OSMX when deleted in OSM.
                #  2. (OK) An element was created and then modified within the diff interval
                logger.warning(
                    "Could not find {0} {1} in db, changing to create".format(
                        action.element.tag, action.element.get("id")
                    )
                )
                a.append(action.element)
                a.set("type", "create")
            else:
                obj_id = action.element.get("id")
                old = ET.SubElement(a, "old")
                new = ET.SubElement(a, "new")
                prev_version = ET.SubElement(old, action.element.tag)
                prev_version.set("id", obj_id)
                rebuild_old_element(prev_version)
                new.append(action.element)

        # 3rd pass
        # Augment the created "old" and "new" elements
        def augment_nd(nd, use_new):
            ll = get_lat_lon(nd.get("ref"), use_new)
            nd.set("lon", ll[0])
            nd.set("lat", ll[1])

        def augment_member(mem, use_new):
            if mem.get("type") == "way":
                ref = mem.get("ref")
                if use_new and ("way/" + ref in actions):
                    way = actions["way/" + ref]
                    for child in way.element:
                        if child.tag == "nd":
                            ll = get_lat_lon(child.get("ref"), use_new)
                            nd = ET.SubElement(mem, "nd")
                            nd.set("lon", ll[0])
                            nd.set("lat", ll[1])
                else:
                    for node_id in ways.get(ref).nodes:
                        ll = get_lat_lon(str(node_id), use_new)
                        nd = ET.SubElement(mem, "nd")
                        nd.set("lon", ll[0])
                        nd.set("lat", ll[1])
            elif mem.get("type") == "node":
                ll = get_lat_lon(mem.get("ref"), use_new)
                mem.set("lon", ll[0])
                mem.set("lat", ll[1])

        def augment(elem, use_new):
            if elem.tag == "way":
                for child in elem:
                    if child.tag == "nd":
                        augment_nd(child, use_new)
            elif elem:
                for child in elem:
                    if child.tag == "member":
                        augment_member(child, use_new)

        for elem in o:
            try:
                if elem.get("type") == "create":
                    augment(elem[0], True)
                else:
                    augment(elem[0][0], False)
                    augment(elem[1][0], True)
            except (TypeError, AttributeError):
                logger.warning(
                    "Changed {0} {1} is incomplete in db".format(
                        elem[1][0].tag, elem[1][0].get("id")
                    )
                )

        # 4th pass:
        # Find changes that propagate to referencing elements:
        # When a node's location changes, that propagates to any ways it belongs to,
        # relations it belongs to and also any relations that the way belongs to.
        # When a way's member list changes, it propagates to any relations it belongs to.
        node_way = osmx.NodeWay(txn)
        node_relation = osmx.NodeRelation(txn)
        way_relation = osmx.WayRelation(txn)

        affected_ways = set()
        affected_relations = set()
        for elem in o:
            if elem.get("type") == "modify":
                if elem[0][0].tag == "node":
                    old_loc = (elem[0][0].get("lat"), elem[0][0].get("lon"))
                    new_loc = (elem[1][0].get("lat"), elem[1][0].get("lon"))
                    if old_loc != new_loc:
                        node_id = elem[0][0].get("id")
                        for rel in node_relation.get(node_id):
                            if "relation/" + str(rel) not in actions:
                                affected_relations.add(rel)
                        for way in node_way.get(node_id):
                            if "way/" + str(way) not in actions:
                                affected_ways.add(way)
                                for rel in way_relation.get(way):
                                    if "relation/" + str(rel) not in actions:
                                        affected_relations.add(rel)

                elif elem[0][0].tag == "way":
                    old_way = [nd.get("ref") for nd in elem[0][0] if nd.tag == "nd"]
                    new_way = [nd.get("ref") for nd in elem[1][0] if nd.tag == "nd"]
                    if old_way != new_way:
                        way_id = elem[0][0].get("id")
                        for rel in way_relation.get(way_id):
                            if "relation/" + str(rel) not in actions:
                                affected_relations.add(rel)

        for w in affected_ways:
            a = ET.SubElement(o, "action")
            a.set("type", "modify")
            old = ET.SubElement(a, "old")
            way_element = ET.SubElement(old, "way")
            way_element.set("id", str(w))
            rebuild_old_element(way_element)

            new = ET.SubElement(a, "new")
            new_elem = copy.deepcopy(way_element)
            new.append(new_elem)
            augment(way_element, False)
            augment(new_elem, True)

        for r in affected_relations:
            old = ET.Element("old")
            relation_element = ET.SubElement(old, "relation")
            relation_element.set("id", str(r))
            rebuild_old_element(relation_element)

            new_elem = copy.deepcopy(relation_element)
            new = ET.Element("new")
            new.append(new_elem)
            try:
                augment(relation_element, False)
                augment(new_elem, True)

                a = ET.SubElement(o, "action")
                a.set("type", "modify")
                a.append(old)
                a.append(new)
            except (TypeError, AttributeError):
                logger.warning("Affected relation {0} is incomplete in db".format(r))

    # 5th pass: add bounding boxes
    for child in o:
        if len(child[0]) > 0:
            if child.get("type") == "create":
                osm_objs = [child[0]]
            else:
                osm_objs = [child[0][0], child[1][0]]
            for osm_obj in osm_objs:
                nds = osm_obj.findall(".//nd")
                if nds:
                    bounds = Bounds()
                    for nd in nds:
                        bounds.add(float(nd.get("lon")), float(nd.get("lat")))
                    osm_obj.insert(0, bounds.elem())

    # 6th pass
    # sort by node, way, relation
    # within each, sorted by increasing ID
    def sort_by_id(x):
        return int(x[0].get("id") if x.get("type") == "create" else x[1][0].get("id"))

    def sort_by_type(x):
        osm_element = x[0] if x.get("type") == "create" else x[1][0]
        if osm_element.tag == "node":
            return 1
        elif osm_element.tag == "way":
            return 2
        return 3

    o[:] = sorted(o, key=sort_by_id)
    o[:] = sorted(o, key=sort_by_type)

    # Set diff metadata
    meta = ET.Element("meta")
    if end_timestamp is not None:
        meta.set("osm_base", end_timestamp.strftime("%Y-%m-%dT%H:%M:%SZ"))
    else:
        logger.warning("No end_timestamp provided, cannot set meta.osm_base!")
    if osc_sequence is not None:
        meta.set("replication_id", str(osc_sequence))
    else:
        logger.warning("No osc_sequence provided, cannot set meta.replication_id!")
    if osc_url is not None:
        meta.set("replication_url", str(osc_url))
    else:
        logger.warning("No osc_url provided, cannot set meta.replication_url!")
    o.insert(0, meta)

    # Set diff note
    note = ET.Element("note")
    note.text = "The data included in this document is from www.openstreetmap.org. The data is made available under ODbL."
    o.insert(0, note)

    indent(o)

    write_xml(ET.ElementTree(o), output_file, logger=logger)
