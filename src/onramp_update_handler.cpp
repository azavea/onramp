#include <stdio.h>
#include <set>
#include <unordered_map>

#include <osmium/builder/builder.hpp>
#include <osmium/builder/attr.hpp>
#include <osmium/handler.hpp>
#include <osmium/memory/buffer.hpp>
#include <osmium/osm.hpp>

#include "osmx/storage.h"
#include "tinyxml2.h"

using namespace std;
using namespace osmium::builder::attr;

enum class adiff_action : int { modify, create, remove };

using BufferOffset = size_t;
using NewOldPair = tuple<adiff_action, BufferOffset, BufferOffset>;

class OnrampUpdateHandler : public osmium::handler::Handler {

public:

  const unsigned long DEFAULT_BUFFER_SIZE = 10240UL;

  OnrampUpdateHandler(MDB_txn *txn) :
    mTxn(txn),
    mLocations(txn),
    mWays(txn,"ways"),
    mRelations(txn,"relations"),
    nodes_buffer{DEFAULT_BUFFER_SIZE},
    ways_buffer{DEFAULT_BUFFER_SIZE},
    relations_buffer{DEFAULT_BUFFER_SIZE} {}

  void node(const osmium::Node& node) {
    if (!node.visible()) {
      node_deleted(node);
    } else if (mLocations.exists(node.id())) {
      node_changed(node);
    } else {
      node_added(node);
    }
  }

  void way(const osmium::Way& way) {
    if (!way.visible()) {
      way_deleted(way);
    } else if (mWays.exists(way.id())) {
      way_changed(way);
    } else {
      way_added(way);
    }
  }

  void relation(const osmium::Relation& relation) {
    if (!relation.visible()) {
      relation_deleted(relation);
    } else if (mRelations.exists(relation.id())) {
      relation_changed(relation);
    } else {
      relation_added(relation);
    }
  }

  void to_aug_diff_xml(string file_name, string valid_at_timestamp) {
    FILE *fp = fopen(file_name.c_str(), "w");
    tinyxml2::XMLPrinter printer(fp);

    // Print XML / osm header
    printer.OpenElement("xml");
    printer.PushAttribute("version", "1.0");
    printer.PushAttribute("encoding", "UTF-8");
    printer.OpenElement("osm");
    printer.PushAttribute("version", "0.6");
    printer.PushAttribute("generator", "onramp v0.0.1");
    printer.OpenElement("meta");
    printer.PushAttribute("osm_base", valid_at_timestamp.c_str());
    printer.CloseElement();

    // Print nodes, ordered by new node id
    vector<NewOldPair> sorted_nodes = map_values(nodes);
    sort(sorted_nodes.begin(), sorted_nodes.end(), [this](NewOldPair& a, NewOldPair& b) {
      const osmium::Node& aNewNode = nodes_buffer.get<osmium::Node>(get<1>(a));
      const osmium::Node& bNewNode = nodes_buffer.get<osmium::Node>(get<1>(b));
      return aNewNode.id() < bNewNode.id();
    });
    for (NewOldPair& pair : sorted_nodes) {
      const adiff_action action = get<0>(pair);
      printer.OpenElement("action");
      printer.PushAttribute("type", adiff_action_str(action).c_str());
      if (action != adiff_action::create) {
        osmium::Node& oldNode = nodes_buffer.get<osmium::Node>(get<2>(pair));
        printer.OpenElement("old");
        node_to_xml(printer, oldNode);
        printer.CloseElement();
      }
      osmium::Node& newNode = nodes_buffer.get<osmium::Node>(get<1>(pair));
      printer.OpenElement("new");
      node_to_xml(printer, newNode);
      printer.CloseElement();
      printer.CloseElement();
    }

    // Print ways, ordered by new way id
    vector<NewOldPair> sorted_ways = map_values(ways);
    sort(sorted_ways.begin(), sorted_ways.end(), [this](NewOldPair& a, NewOldPair& b) {
      const osmium::Way& aNewWay = ways_buffer.get<osmium::Way>(get<1>(a));
      const osmium::Way& bNewWay = ways_buffer.get<osmium::Way>(get<1>(b));
      return aNewWay.id() < bNewWay.id();
    });
    for (NewOldPair& pair : sorted_ways) {
      const adiff_action action = get<0>(pair);
      printer.OpenElement("action");
      printer.PushAttribute("type", adiff_action_str(action).c_str());
      if (action != adiff_action::create) {
        osmium::Way& oldWay = ways_buffer.get<osmium::Way>(get<2>(pair));
        printer.OpenElement("old");
        way_to_xml(printer, oldWay);
        printer.CloseElement();
      }
      osmium::Way& newWay = ways_buffer.get<osmium::Way>(get<1>(pair));
      printer.OpenElement("new");
      way_to_xml(printer, newWay);
      printer.CloseElement();
      printer.CloseElement();
    }

    // Print relations, ordered by new relation id
    vector<NewOldPair> sorted_relations = map_values(relations);
    sort(sorted_relations.begin(), sorted_relations.end(), [this](NewOldPair& a, NewOldPair& b) {
      const osmium::Relation& aRelation = relations_buffer.get<osmium::Relation>(get<1>(a));
      const osmium::Relation& bRelation = relations_buffer.get<osmium::Relation>(get<1>(b));
      return aRelation.id() < bRelation.id();
    });
    for (NewOldPair& pair : sorted_relations) {
      const adiff_action action = get<0>(pair);
      printer.OpenElement("action");
      printer.PushAttribute("type", adiff_action_str(action).c_str());
      if (action != adiff_action::create) {
        osmium::Relation& oldRelation = relations_buffer.get<osmium::Relation>(get<2>(pair));
        printer.OpenElement("old");
        const auto& old_offsets = old_relation_member_offsets.at(oldRelation.id());
        relation_to_xml(printer, oldRelation, old_offsets);
        printer.CloseElement();
      }
      osmium::Relation& newRelation = relations_buffer.get<osmium::Relation>(get<1>(pair));
      printer.OpenElement("new");
      const auto& new_offsets = new_relation_member_offsets.at(newRelation.id());
      relation_to_xml(printer, newRelation, new_offsets);
      printer.CloseElement();
      printer.CloseElement();
    }

    printer.CloseElement();
    printer.CloseElement();

    fclose(fp);
  }

private:
  MDB_txn *mTxn;
  osmx::db::Locations mLocations;
  osmx::db::Elements mWays;
  osmx::db::Elements mRelations;

  osmium::memory::Buffer nodes_buffer;
  unordered_map<osmium::object_id_type, NewOldPair> nodes;

  osmium::memory::Buffer ways_buffer;
  unordered_map<osmium::object_id_type, NewOldPair> ways;

  osmium::memory::Buffer relations_buffer;
  unordered_map<osmium::object_id_type, unordered_map<osmium::object_id_type, BufferOffset>> old_relation_member_offsets;
  unordered_map<osmium::object_id_type, unordered_map<osmium::object_id_type, BufferOffset>> new_relation_member_offsets;
  unordered_map<osmium::object_id_type, NewOldPair> relations;

  template <typename K, typename V> vector<V> map_values(unordered_map<K, V> the_map) {
    vector<V> values;
    values.reserve(the_map.size());
    for (auto& entry : the_map) {
      values.push_back(entry.second);
    }
    return values;
  }

  vector<NewOldPair> pairs_by(unordered_map<osmium::object_id_type, NewOldPair>& osm_object_map, adiff_action t) {
    vector<NewOldPair> osm_objects = map_values(osm_object_map);
    vector<NewOldPair> filtered_objects;
    for (auto& entry : osm_objects) {
      if (get<0>(entry) == t) {
        filtered_objects.push_back(entry);
      }
    }
    return filtered_objects;
  }

  string adiff_action_str(adiff_action action) {
    switch (action) {
      case adiff_action::create: return "create";
      case adiff_action::modify: return "modify";
      case adiff_action::remove: return "delete";
      default:
        assert(false);
        return "";
    }
  }

  osmium::item_type to_osmium_type(RelationMember::Type relation_type) {
    switch (relation_type) {
      case RelationMember::Type::NODE: return osmium::item_type::node;
      case RelationMember::Type::WAY: return osmium::item_type::way;
      case RelationMember::Type::RELATION: return osmium::item_type::relation;
      default:
        assert(false);
        return osmium::item_type::undefined;
    }
  }

  void node_added(const osmium::Node& node) {
    size_t offset = create_new_node(nodes_buffer, node);
    nodes.insert({node.id(), make_tuple(adiff_action::create, offset, NULL)});
  }

  void node_changed(const osmium::Node& node) {
    size_t newOffset = create_new_node(nodes_buffer, node);
    size_t oldOffset = create_old_node(nodes_buffer, node.id());

    nodes.insert({node.id(), make_tuple(adiff_action::modify, newOffset, oldOffset)});
  }

  void node_deleted(const osmium::Node& node) {
    size_t newOffset = create_new_node(nodes_buffer, node);
    size_t oldOffset = create_old_node(nodes_buffer, node.id());

    nodes.insert({node.id(), make_tuple(adiff_action::remove, newOffset, oldOffset)});
  }

  void way_added(const osmium::Way& way) {
    size_t newOffset = create_new_way(ways_buffer, way);
    ways.insert({way.id(), make_tuple(adiff_action::create, newOffset, NULL)});
  }

  void way_changed(const osmium::Way& way) {
    size_t newOffset = create_new_way(ways_buffer, way);
    size_t oldOffset = create_old_way(ways_buffer, way.id());

    ways.insert({way.id(), make_tuple(adiff_action::modify, newOffset, oldOffset)});
  }

  void way_deleted(const osmium::Way& way) {
    size_t newOffset = create_new_way(ways_buffer, way);
    size_t oldOffset = create_old_way(ways_buffer, way.id());

    ways.insert({way.id(), make_tuple(adiff_action::remove, newOffset, oldOffset)});
  }

  void relation_added(const osmium::Relation& relation) {
    size_t newOffset = create_new_relation(relations_buffer, relation);
    relations.insert({relation.id(), make_tuple(adiff_action::create, newOffset, NULL)});
  }

  void relation_changed(const osmium::Relation& relation) {
    size_t newOffset = create_new_relation(relations_buffer, relation);
    size_t oldOffset = create_old_relation(relations_buffer, relation.id());

    relations.insert({relation.id(), make_tuple(adiff_action::modify, newOffset, oldOffset)});
  }

  void relation_deleted(const osmium::Relation& relation) {
    size_t newOffset = create_new_relation(relations_buffer, relation);
    size_t oldOffset = create_old_relation(relations_buffer, relation.id());

    relations.insert({relation.id(), make_tuple(adiff_action::remove, newOffset, oldOffset)});
  }

  /**
   * Create a osmium::Node object from the node retrieved from OSC
   *
   * This is simply a copy from one buffer to another, as a node is a single chunk of memory
   */
  BufferOffset create_new_node(osmium::memory::Buffer& buffer, const osmium::Node& node) {
    buffer.add_item(node);
    return buffer.commit();
  }

  /**
   * Create a osmium::Node object from the old state in OSMX
   */
  BufferOffset create_old_node(osmium::memory::Buffer& buffer,
                               const osmium::object_id_type nodeId) {
    osmx::db::Location oldLocation = mLocations.get(nodeId);
    return osmium::builder::add_node(
      nodes_buffer,
      _id(nodeId),
      _location(oldLocation.coords.lon(), oldLocation.coords.lat())
    );
  }

  /**
   * Create a osmium::Way object from the partially complete way retrieved from OSC
   *
   * Backfills missing child objects from OSMX, as any that weren't found by the handler at this
   * point weren't changed.
   */
  BufferOffset create_new_way(osmium::memory::Buffer& buffer,
                            const osmium::Way& way) {
    {
      osmium::builder::WayBuilder builder{buffer};
      builder.set_id(way.id());
      builder.set_changeset(way.changeset());
      builder.set_timestamp(way.timestamp());
      builder.set_uid(way.uid());
      builder.set_user(way.user());
      builder.set_version(way.version());
      builder.set_visible(way.visible());
      {
        osmium::builder::TagListBuilder tl_builder{buffer, &builder};
        for (auto& tag : way.tags()) {
          tl_builder.add_tag(tag.key(), tag.value());
        }
      }
      {
        osmium::builder::WayNodeListBuilder wnl_builder{buffer, &builder};
        for (auto& nodeRef : way.nodes()) {
          const auto node_id = nodeRef.ref();
          // If new way has a node with a location defined, it was changed in the OSC file so use it
          if (nodeRef.location().is_defined()) {
            wnl_builder.add_node_ref(nodeRef);
          // Otherwise fall back to the old node location from osmx
          } else {
            osmx::db::Location oldLoc = mLocations.get(node_id);
            wnl_builder.add_node_ref(osmium::NodeRef(node_id, oldLoc.coords));
          }
        }
      }
    }
    return buffer.commit();
  }

  /**
   * Create a osmium::Way object from the old state in OSMX
   */
  BufferOffset create_old_way(osmium::memory::Buffer& buffer,
                              osmium::object_id_type wayId) {
    set<uint64_t> prev_nodes;
    bool mWayExists = mWays.exists(wayId);
    if (mWayExists) {
      auto reader = mWays.getReader(wayId);
      Way::Reader mWay = reader.getRoot<Way>();
      for (auto const &node_id : mWay.getNodes()) {
        prev_nodes.insert(node_id);
      }
    }

    {
      osmium::builder::WayBuilder builder{buffer};
      builder.set_id(wayId);
      {
        osmium::builder::WayNodeListBuilder wnl_builder{buffer, &builder};
        for (uint64_t node_id : prev_nodes) {
          osmx::db::Location loc = mLocations.get(node_id);
          wnl_builder.add_node_ref(osmium::NodeRef(node_id, loc.coords));
        }
      }
    }
    return buffer.commit();
  }

  /**
   * Create a osmium::Relation object from the partially complete way retrieved from OSC
   *
   * Backfills missing child objects from OSMX, as any that weren't found by the handler at this
   * point weren't changed.
   */
  BufferOffset create_new_relation(osmium::memory::Buffer& buffer,
                                   const osmium::Relation& relation) {
    {
      osmium::builder::RelationBuilder builder{buffer};
      builder.set_id(relation.id());
      builder.set_changeset(relation.changeset());
      builder.set_timestamp(relation.timestamp());
      builder.set_uid(relation.uid());
      builder.set_user(relation.user());
      builder.set_version(relation.version());
      builder.set_visible(relation.visible());

      {
        osmium::builder::TagListBuilder tl_builder{buffer, &builder};
        for (auto& tag : relation.tags()) {
          tl_builder.add_tag(tag.key(), tag.value());
        }
      }

      {
        osmium::builder::RelationMemberListBuilder rml_builder{buffer, &builder};
        unordered_map<osmium::object_id_type, BufferOffset> member_offsets;
        for (const auto& member : relation.members()) {
          const osmium::item_type member_type = member.type();
          if (member_type == osmium::item_type::node) {
            BufferOffset node_offset;
            try {
              const NewOldPair& nodePair = nodes.at(member.ref());
              node_offset = get<1>(nodePair);
            } catch (out_of_range& e) {
              node_offset = create_old_node(nodes_buffer, member.ref());
            }
            member_offsets.insert({member.ref(), node_offset});
          } else if (member_type == osmium::item_type::way) {
            BufferOffset way_offset;
            try {
              const NewOldPair& wayPair = ways.at(member.ref());
              way_offset = get<1>(wayPair);
            } catch (out_of_range& e) {
              way_offset = create_old_way(ways_buffer, member.ref());
            }
            member_offsets.insert({member.ref(), way_offset});
          }
          rml_builder.add_member(member_type, member.ref(), member.role(), nullptr);
        }
        new_relation_member_offsets.insert({relation.id(), member_offsets});
      }
    }
    return buffer.commit();
  }

  /**
   * Create a osmium::Relation object from the old state in OSMX
   */
  BufferOffset create_old_relation(osmium::memory::Buffer& buffer,
                                   osmium::object_id_type relationId) {
    {
      osmium::builder::RelationBuilder builder{buffer};
      builder.set_id(relationId);

      auto reader = mRelations.getReader(relationId);
      Relation::Reader relation = reader.getRoot<Relation>();
      {
        osmium::builder::RelationMemberListBuilder rml_builder{buffer, &builder};
        unordered_map<osmium::object_id_type, BufferOffset> member_offsets;
        for (auto const &member : relation.getMembers()) {
          const osmium::object_id_type member_ref = member.getRef();
          const osmium::item_type member_type = to_osmium_type(member.getType());
          if (member_type == osmium::item_type::node) {
            BufferOffset node_offset = create_old_node(nodes_buffer, member_ref);
            member_offsets.insert({member_ref, node_offset});
          } else if (member_type == osmium::item_type::way) {
            BufferOffset way_offset = create_old_way(ways_buffer, member_ref);
            member_offsets.insert({member_ref, way_offset});
          }

          rml_builder.add_member(member_type, member_ref, member.getRole(), nullptr);
        }
        old_relation_member_offsets.insert({relationId, member_offsets});
      }
    }
    return buffer.commit();
  }

  void node_to_xml(tinyxml2::XMLPrinter& printer, osmium::Node& node) {
    printer.OpenElement("node");
    printer.PushAttribute("id", node.id());
    if (node.visible()) {
      printer.PushAttribute("lat", node.location().lat());
      printer.PushAttribute("lon", node.location().lon());
    } else {
      printer.PushAttribute("visible", node.visible());
    }
    printer.PushAttribute("version", node.version());
    printer.PushAttribute("timestamp", node.timestamp().to_iso().c_str());
    if (node.changeset()) {
      printer.PushAttribute("changeset", node.changeset());
    }
    if (node.uid()) {
      printer.PushAttribute("uid", node.uid());
    }
    if (strlen(node.user()) != 0) {
      printer.PushAttribute("user", node.user());
    }

    for (auto& tag : node.tags()) {
      printer.OpenElement("tag");
      printer.PushAttribute("k", tag.key());
      printer.PushAttribute("v", tag.value());
      printer.CloseElement();
    }
    printer.CloseElement();
  };

  void way_to_xml(tinyxml2::XMLPrinter& printer, osmium::Way& way) {
    printer.OpenElement("way");
    printer.PushAttribute("id", way.id());
    printer.PushAttribute("version", way.version());
    printer.PushAttribute("timestamp", way.timestamp().to_iso().c_str());
    if (way.changeset()) {
      printer.PushAttribute("changeset", way.changeset());
    }
    if (way.uid()) {
      printer.PushAttribute("uid", way.uid());
    }
    if (strlen(way.user()) != 0) {
      printer.PushAttribute("user", way.user());
    }

    if (way.visible()) {
      const osmium::Box bounds = way.envelope();
      printer.OpenElement("bounds");
      printer.PushAttribute("minlat", bounds.bottom_left().lat());
      printer.PushAttribute("minlon", bounds.bottom_left().lon());
      printer.PushAttribute("maxlat", bounds.top_right().lat());
      printer.PushAttribute("maxlon", bounds.top_right().lon());
      printer.CloseElement();

      for (auto& node : way.nodes()) {
        printer.OpenElement("nd");
        printer.PushAttribute("ref", node.ref());
        printer.PushAttribute("lat", node.lat());
        printer.PushAttribute("lon", node.lon());
        printer.CloseElement();
      }
    } else {
      printer.PushAttribute("visible", way.visible());
    }

    for (auto& tag : way.tags()) {
      printer.OpenElement("tag");
      printer.PushAttribute("k", tag.key());
      printer.PushAttribute("v", tag.value());
      printer.CloseElement();
    }
    printer.CloseElement();
  }

  void relation_to_xml(tinyxml2::XMLPrinter& printer,
                       osmium::Relation& relation,
                       unordered_map<osmium::object_id_type, BufferOffset> relation_member_offsets) {
    printer.OpenElement("relation");
    printer.PushAttribute("id", relation.id());
    printer.PushAttribute("version", relation.version());
    printer.PushAttribute("timestamp", relation.timestamp().to_iso().c_str());
    if (relation.changeset()) {
      printer.PushAttribute("changeset", relation.changeset());
    }
    if (relation.uid()) {
      printer.PushAttribute("uid", relation.uid());
    }
    if (strlen(relation.user()) != 0) {
      printer.PushAttribute("user", relation.user());
    }

    if (relation.visible()) {
      // TODO: UGH we have to compute envelope ourselves?
      // const osmium::Box bounds = relation.envelope();
      // printer.OpenElement("bounds");
      // printer.PushAttribute("minlat", bounds.bottom_left().lat());
      // printer.PushAttribute("minlon", bounds.bottom_left().lon());
      // printer.PushAttribute("maxlat", bounds.top_right().lat());
      // printer.PushAttribute("maxlon", bounds.top_right().lon());
      // printer.CloseElement();

      for (auto& member : relation.members()) {
        const auto member_type = member.type();
        printer.OpenElement("member");
        printer.PushAttribute("type", osmium::item_type_to_name(member_type));
        printer.PushAttribute("ref", member.ref());
        printer.PushAttribute("role", member.role());
        if (member_type == osmium::item_type::node) {
          const BufferOffset member_offset = relation_member_offsets.at(member.ref());
          const osmium::Node& node = nodes_buffer.get<osmium::Node>(member_offset);
          if (node.location().valid()) {
            printer.PushAttribute("lat", node.location().lat());
            printer.PushAttribute("lon", node.location().lon());

          }
        } else if (member_type == osmium::item_type::way) {
          const BufferOffset member_offset = relation_member_offsets.at(member.ref());
          const osmium::Way& way = ways_buffer.get<osmium::Way>(member_offset);
          for (const auto& node : way.nodes()) {
            printer.OpenElement("nd");
            if (node.location().valid()) {
              printer.PushAttribute("lat", node.location().lat());
              printer.PushAttribute("lon", node.location().lon());
            }
            printer.CloseElement();
          }
        }
        printer.CloseElement();
      }
    } else {
      printer.PushAttribute("visible", relation.visible());
    }

    for (auto& tag : relation.tags()) {
      printer.OpenElement("tag");
      printer.PushAttribute("k", tag.key());
      printer.PushAttribute("v", tag.value());
      printer.CloseElement();
    }
    printer.CloseElement();

  }
};
