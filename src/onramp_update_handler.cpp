#include "./osmx_update_handler.cpp"

class OnrampUpdateHandler : public OsmxUpdateHandler {

public:
  OnrampUpdateHandler(MDB_txn *txn) : OsmxUpdateHandler(txn) {};

  // The oldNode is coming from osmx and will only contain an id and location
  virtual void node_changed(const osmium::Node& newNode, const osmium::Location& oldLocation) {
    cout << "NODE CHANGED: " << newNode.id() << " to v" << newNode.version() << endl;
  }

  virtual void node_added(const osmium::Node& newNode) {
    cout << "NODE ADDED: " << to_string(newNode.id()) << endl;
  }

  virtual void node_deleted(const osmium::Node& oldNode) {
    cout << "NODE DELETED: " << to_string(oldNode.id()) << endl;
  }
};
