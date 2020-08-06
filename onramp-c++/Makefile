.DEFAULT_GOAL=onramp

DEBUG=-g

LIBLMDB=vendor/OSMExpress/vendor/lmdb/libraries/liblmdb/liblmdb.a
LIBCAPNP=vendor/OSMExpress/vendor/capnproto/c++/src/capnp/libcapnp.a
LIBKJ=vendor/OSMExpress/vendor/capnproto/c++/src/kj/libkj.a
LIBROARING=vendor/OSMExpress/vendor/CRoaring/src/libroaring.a
LIBS2=vendor/OSMExpress/vendor/s2geometry/libs2.a

OSMEXPRESS_SOURCE_FILES=./vendor/OSMExpress/src/storage.cpp
TINYXML2_SOURCE_FILES=./vendor/TinyXML2/tinyxml2.cpp
SOURCE_FILES=./src/main.cpp ./src/onramp_update_handler.cpp ./src/onramp_relations_manager.cpp $(OSMEXPRESS_SOURCE_FILES) $(TINYXML2_SOURCE_FILES)

$(LIBLMDB) $(LIBCAPNP) $(LIBKJ) $(LIBROARING) $(LIBS2):
	cd vendor/OSMExpress && \
	cmake -DCMAKE_BUILD_TYPE=Release . && \
	make

osmx_test: osmx_test.cpp $(LIBLMDB) $(LIBCAPNP) $(LIBKJ) $(LIBROARING) $(LIBS2)
	g++ $(DEBUG) -std=c++14 -static \
	  -I vendor/TinyXML2 \
	  -I vendor/OSMExpress/include \
		-I vendor/OSMExpress/vendor/lmdb/libraries/liblmdb \
		-I vendor/OSMExpress/vendor/libosmium/include \
		-I vendor/OSMExpress/vendor/capnproto/c++/src \
		-I vendor/OSMExpress/vendor/s2geometry/src \
		-I vendor/OSMExpress/vendor/CRoaring/cpp \
		-I vendor/OSMExpress/vendor/CRoaring/include \
		-I vendor/OSMExpress/vendor/protozero/include \
		-I vendor/OSMExpress/vendor/cxxopts/include \
		-L vendor/OSMExpress/vendor/lmdb/libraries/liblmdb \
		-L vendor/OSMExpress/vendor/capnproto/c++/src/capnp \
		-L vendor/OSMExpress/vendor/capnproto/c++/src/kj \
		-L vendor/OSMExpress/vendor/CRoaring/src \
		-L vendor/OSMExpress/vendor/s2geometry \
		osmx_test.cpp \
		-lbz2 -lcapnp -lexpat -lkj -llmdb -lpthread -lroaring -ls2 -lz \
		-o osmx_test

onramp : $(SOURCE_FILES) $(LIBLMDB) $(LIBCAPNP) $(LIBKJ) $(LIBROARING) $(LIBS2)
	g++ $(DEBUG) -std=c++14 -static \
	  -I vendor/TinyXML2 \
	  -I vendor/OSMExpress/include \
		-I vendor/OSMExpress/vendor/lmdb/libraries/liblmdb \
		-I vendor/OSMExpress/vendor/libosmium/include \
		-I vendor/OSMExpress/vendor/capnproto/c++/src \
		-I vendor/OSMExpress/vendor/s2geometry/src \
		-I vendor/OSMExpress/vendor/CRoaring/cpp \
		-I vendor/OSMExpress/vendor/CRoaring/include \
		-I vendor/OSMExpress/vendor/protozero/include \
		-I vendor/OSMExpress/vendor/cxxopts/include \
		-L vendor/OSMExpress/vendor/lmdb/libraries/liblmdb \
		-L vendor/OSMExpress/vendor/capnproto/c++/src/capnp \
		-L vendor/OSMExpress/vendor/capnproto/c++/src/kj \
		-L vendor/OSMExpress/vendor/CRoaring/src \
		-L vendor/OSMExpress/vendor/s2geometry \
		$(SOURCE_FILES) \
		-lbz2 -lcapnp -lexpat -lkj -llmdb -lpthread -lroaring -ls2 -lz \
		-o onramp

clean :
	rm $(LIBLMDB)
	rm $(LIBCAPNP)
	rm $(LIBKJ)
	rm $(LIBROARING)
	rm $(LIBS2)
	rm ./onramp
