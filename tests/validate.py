import argparse
from collections import Counter
import gzip
import json
from pathlib import Path
import xml.etree.ElementTree as ET


def list_duplicates(elements):
    """ Return list of elements that are visible more than once in the input elements. """
    return [item for item, count in Counter(elements).items() if count > 1]


def elements_equal(e1, e2):
    """ Equality comparison for two xml.etree.ElementTree.Element

    From https://stackoverflow.com/a/24349916

    """
    if e1.tag != e2.tag:
        return False
    if (
        (
            e1.text is not None
            and e2.text is not None
            and e1.text.strip() != e2.text.strip()
        )
        or (e1.text is None and e2.text is not None)
        or (e1.text is not None and e2.text is None)
    ):
        return False
    if (
        (
            e1.tail is not None
            and e2.tail is not None
            and e1.tail.strip() != e2.tail.strip()
        )
        or (e1.tail is None and e2.tail is not None)
        or (e1.tail is not None and e2.tail is None)
    ):
        return False
    if e1.attrib != e2.attrib:
        return False
    if len(e1) != len(e2):
        return False
    return all(elements_equal(c1, c2) for c1, c2 in zip(e1, e2))


def elements_from_root(root_element):
    """ Retrieve list of unique tuples for each element

    (
        "create"|"modify"|"delete",
        "old"|"new",
        "node"|"way"|"relation",
        object_id,
        object_version
    )

    Boy this processing is a pain because create skips the intermediate
    <old>|<new> element.

    """
    osm_types = set(["node", "way", "relation"])

    elements = []
    for action in root_element:
        action_type = action.get("type")
        if action_type is None:
            continue

        new = action.find("new")
        old = action.find("old")
        if new is not None:
            elements += [
                (action_type, "new", e.tag, e.get("id"), e.get("version"),)
                for e in new
                if e.tag in osm_types
            ]
        if old is not None:
            elements += [
                (action_type, "old", e.tag, e.get("id"), e.get("version"),)
                for e in old
                if e.tag in osm_types
            ]
        if new is None and old is None:
            elements += [
                (action_type, "new", e.tag, e.get("id"), e.get("version"),)
                for e in action
                if e.tag in osm_types
            ]
    return elements


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--onramp-diff", type=str)
    parser.add_argument("--overpass-diff", type=Path, default=None)
    parser.add_argument("--detailed", action="store_true")
    args = parser.parse_args()

    # Load onramp diff from local path
    with gzip.open(args.onramp_diff) as fp:
        onramp_root_element = ET.parse(fp).getroot()
    onramp_elements = elements_from_root(onramp_root_element)
    onramp_elements_set = set(onramp_elements)

    # Open and parse Overpass API augmented diff
    overpass_root_element = ET.parse(str(args.overpass_diff)).getroot()
    overpass_elements = elements_from_root(overpass_root_element)
    overpass_elements_set = set(overpass_elements)

    results = {"both": {}, "onramp": {}, "overpass": {}}
    # Set element counts
    results["both"]["count"] = len(
        onramp_elements_set.intersection(overpass_elements_set)
    )
    results["onramp"]["count"] = len(onramp_elements_set)
    results["overpass"]["count"] = len(overpass_elements_set)

    # Are onramp elements a subset of overpass?
    results["onramp"]["is_subset"] = onramp_elements_set <= overpass_elements_set
    # Elements in onramp that aren't in overpass
    onramp_difference = onramp_elements_set.difference(overpass_elements_set)
    # results["onramp"]["difference"] = list(onramp_difference)[:10]
    # Count of elements in onramp that aren't in overpass
    results["onramp"]["difference_count"] = len(onramp_difference)
    # List duplicate elements
    results["onramp"]["duplicates"] = list_duplicates(onramp_elements)

    # Are overpass elements a subset of onramp?
    results["overpass"]["is_subset"] = overpass_elements_set <= onramp_elements_set
    # Elements in overpass that aren't in onramp
    overpass_difference = overpass_elements_set.difference(onramp_elements_set)
    # results["overpass"]["difference"] = list(overpass_difference)[:10]
    # Count of elements in overpass that aren't in onramp
    results["overpass"]["difference_count"] = len(overpass_difference)
    # List duplicate elements
    results["overpass"]["duplicates"] = list_duplicates(overpass_elements)

    # Print comparison summary
    print(json.dumps(results, indent=2))

    intersection = onramp_elements_set.intersection(overpass_elements_set)
    if args.detailed:
        equal_elements = set()
        different_elements = set()
        for element in intersection:
            if element[0] == "create":
                onramp_element = onramp_root_element.find(
                    "./action[@type='{}']/{}[@id='{}'][@version='{}']".format(
                        element[0], element[2], element[3], element[4]
                    )
                )
                overpass_element = overpass_root_element.find(
                    "./action[@type='{}']/{}[@id='{}'][@version='{}']".format(
                        element[0], element[2], element[3], element[4]
                    )
                )
            else:
                onramp_element = onramp_root_element.find(
                    "./action[@type='{}']/{}/{}[@id='{}'][@version='{}']".format(
                        *element
                    )
                )
                overpass_element = overpass_root_element.find(
                    "./action[@type='{}']/{}/{}[@id='{}'][@version='{}']".format(
                        *element
                    )
                )
            if (
                onramp_element is not None
                and overpass_element is not None
                and elements_equal(onramp_element, overpass_element)
            ):
                equal_elements.add(element)
            else:
                # print(element)
                different_elements.add(element)

        # print(equal_elements)
        print("{}/{} elements are equal".format(len(equal_elements), len(intersection)))
        print("Different: {}".format(list(different_elements)[:20]))


if __name__ == "__main__":
    main()
