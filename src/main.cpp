#include <iostream>

#include "cxxopts.hpp"
#include "osmium/handler.hpp"
#include "osmium/io/any_input.hpp"
#include "osmx/storage.h"

#include "./onramp_update_handler.cpp"

using namespace std;

int main(int argc, char* argv[]) {
  cxxopts::Options cmdoptions("OnRamp", "Generate augmented diff from osc change file.");
  cmdoptions.add_options()
    ("v,verbose", "Verbose output")
    ("commit", "Commit the update")
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
    cout << " onramp planet.osmx 123456.osc 123456 2019-09-05T00:00:00Z --commit" << endl << endl;
    cout << "OPTIONS:" << endl;
    cout << " --v,--verbose: verbose output." << endl;
    cout << " --commit: Actually commit the transaction; otherwise runs the update and rolls back." << endl;
    exit(1);
  }

  string osmx = result["osmx"].as<string>();
  string osc = result["osc"].as<string>();
  bool verbose = result.count("verbose") > 0;
  auto startTime = std::chrono::high_resolution_clock::now();

  MDB_env* env = osmx::db::createEnv(osmx,true);
  MDB_txn* txn;
  CHECK(mdb_txn_begin(env, NULL, 0, &txn));

  string old_seqnum = "UNKNOWN";
  auto new_seqnum = result["seqnum"].as<string>();
  auto new_timestamp = result["timestamp"].as<string>();
  osmx::db::Metadata metadata(txn);
  if (verbose) cout << "Timestamp: " << metadata.get("osmosis_replication_timestamp") << endl;
  old_seqnum = metadata.get("osmosis_replication_sequence_number");

  if (verbose) cout << "Starting update from " << old_seqnum << " to " << new_seqnum << endl;
  const osmium::io::File input_file{osc};

  osmium::io::Reader reader{input_file, osmium::osm_entity_bits::object};
  OnrampUpdateHandler handler(txn);
  osmium::apply(reader, handler);

  auto duration = (std::chrono::duration_cast<std::chrono::milliseconds>( std::chrono::high_resolution_clock::now() - startTime ).count()) / 1000.0;

  if (result.count("commit") > 0) {
    {
      metadata.put("osmosis_replication_sequence_number",new_seqnum);
      metadata.put("osmosis_replication_timestamp",new_timestamp);
    }
    CHECK(mdb_txn_commit(txn));
    cout << "Committed: ";
  } else {
    mdb_txn_abort(txn);
    cout << "Aborted: ";
  }
  cout << old_seqnum << " -> " << new_seqnum << " in " << duration << " seconds." << endl;
  mdb_env_sync(env,true);
  mdb_env_close(env);
}
