#include <osmium/builder/builder.hpp>
#include <osmium/builder/attr.hpp>
#include <osmium/relations/relations_manager.hpp>

using namespace std;
using namespace osmium::builder::attr;

// Load everything, and just pass each relation through to the buffer
class OnrampRelationsManager : public osmium::relations::RelationsManager<OnrampRelationsManager, true, true, true> {

public:
  void complete_relation(osmium::Relation& relation) {
    osmium::memory::Buffer& buffer = this->buffer();
    buffer.add_item(relation);
    buffer.commit();
  }
};
