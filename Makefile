DEBUG=-g

onramp : osmexpress
	g++ $(DEBUG) -std=c++14 -static \
	  -I vendor/OSMExpress/include \
		-I vendor/OSMExpress/vendor/lmdb/libraries/liblmdb \
		-I vendor/OSMExpress/vendor/libosmium/include \
		-I vendor/OSMExpress/vendor/capnproto/c++/src \
		-I vendor/OSMExpress/vendor/s2geometry/src \
		-I vendor/OSMExpress/vendor/CRoaring/cpp \
		-I vendor/OSMExpress/vendor/CRoaring/include \
		-I vendor/OSMExpress/vendor/protozero/include \
		-I vendor/OSMExpress/vendor/cxxopts/include \
		-I ./include \
		-L vendor/OSMExpress/vendor/lmdb/libraries/liblmdb \
		-L vendor/OSMExpress/vendor/capnproto/c++/src/capnp \
		-L vendor/OSMExpress/vendor/capnproto/c++/src/kj \
		-L vendor/OSMExpress/vendor/CRoaring/src \
		-L vendor/OSMExpress/vendor/s2geometry \
		./src/main.cpp ./src/osmx_update_handler.cpp ./vendor/OSMExpress/src/storage.cpp \
		-lbz2 -lcapnp -lexpat -lkj -llmdb -lpthread -lroaring -ls2 -lz \
		-o onramp

osmexpress :
	cd vendor/OSMExpress && \
	cmake -DCMAKE_BUILD_TYPE=Release . && \
	make
