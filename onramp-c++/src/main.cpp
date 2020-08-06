#include <iostream>

#include <cxxopts.hpp>
#include <osmium/handler.hpp>
#include <osmium/handler/node_locations_for_ways.hpp>
#include <osmium/index/map/flex_mem.hpp>
#include <osmium/io/any_input.hpp>
#include <osmx/storage.h>

#include "./onramp_relations_manager.cpp"
#include "./onramp_update_handler.cpp"

using namespace std;

using index_type = osmium::index::map::FlexMem<osmium::unsigned_object_id_type, osmium::Location>;
using location_handler_type = osmium::handler::NodeLocationsForWays<index_type>;

int main(int argc, char* argv[]) {
  cxxopts::Options cmdoptions("OnRamp", "Generate augmented diff from osc change file.");
  cmdoptions.add_options()
    ("v,verbose", "Verbose output")
    ("osmx", ".osmx to update", cxxopts::value<string>())
    ("osc", ".osc to apply", cxxopts::value<string>())
    ("seqnum", "The sequence number of the .osc", cxxopts::value<string>())
    ("timestamp", "The timestamp of the .osc", cxxopts::value<string>())
  ;

  cmdoptions.parse_positional({"osmx","osc","seqnum","timestamp"});
  auto result = cmdoptions.parse(argc, argv);

  if (result.count("osmx") == 0 || result.count("osc") == 0 || \
    result.count("seqnum") == 0 || result.count("timestamp") == 0) {
    cout << "Usage: onramp OSMX_FILE OSC_FILE SEQNUM TIMESTAMP [OPTIONS]" << endl;
    cout << "Generates aug diffs from osm change file before applying changes to osmx database." << endl << endl;
    cout << "EXAMPLE:" << endl;
    cout << " onramp planet.osmx 123456.osc 123456 2019-09-05T00:00:00Z" << endl << endl;
    cout << "OPTIONS:" << endl;
    cout << " --v,--verbose: verbose output." << endl;
    exit(1);
  }

  string osmx = result["osmx"].as<string>();
  string osc = result["osc"].as<string>();
  bool verbose = result.count("verbose") > 0;
  auto startTime = std::chrono::high_resolution_clock::now();

  MDB_txn* txn;
  MDB_env* roEnv = osmx::db::createEnv(osmx,false);
  CHECK(mdb_txn_begin(roEnv, NULL, MDB_RDONLY, &txn));

  // osmx::db::Locations mLocations(txn);
  // uint64_t testId = 6892919079;
  // osmx::db::Location oldLoc = mLocations.get(testId);
  // cout << "Node " << testId << ": (" << oldLoc.coords.lon() << ", " << oldLoc.coords.lat() << ")" << endl;
  // exit;

  string old_seqnum = "UNKNOWN";
  auto new_seqnum = result["seqnum"].as<string>();
  auto new_timestamp = result["timestamp"].as<string>();
  osmx::db::Metadata metadata(txn);
  if (verbose) cout << "Timestamp: " << metadata.get("osmosis_replication_timestamp") << endl;
  old_seqnum = metadata.get("osmosis_replication_sequence_number");

  if (verbose) cout << "Starting update from " << old_seqnum << " to " << new_seqnum << endl;
  const osmium::io::File input_file{osc};

  // First: Build relation ojects using osmium RelationManager subclass
  OnrampRelationsManager relationsManager;
  osmium::relations::read_relations(input_file, relationsManager);

  // Second: construct augmented diff using old osmx db state + new osc change file
  //         We use two passes so that the osmx database isn't updated in place before
  //         we read all old objects from it.
  osmium::io::Reader reader{input_file, osmium::osm_entity_bits::node|osmium::osm_entity_bits::way};
  index_type index;
  location_handler_type location_handler(index);
  location_handler.ignore_errors();
  OnrampUpdateHandler handler(txn);
  osmium::apply(reader, location_handler, handler, relationsManager.handler([&](osmium::memory::Buffer&& buffer) {
    osmium::apply(buffer, handler);
  }));
  reader.close();

  // Third: Create complete relation objects from incomplete relation state in osc file
  relationsManager.for_each_incomplete_relation([&](const osmium::relations::RelationHandle& handle) {
    const osmium::Relation& relation = *handle;
    handler.relation(relation);
  });

  string adiff_filename = "./" + new_seqnum + ".adiff.xml";
  handler.to_aug_diff_xml(adiff_filename, new_timestamp);

  auto duration = (std::chrono::duration_cast<std::chrono::milliseconds>( std::chrono::high_resolution_clock::now() - startTime ).count()) / 1000.0;
  cout << "Augmented diff " << new_seqnum << " generated in " << duration << " seconds." << endl;

  mdb_env_close(roEnv);
}
