
onramp : osmexpress
	g++ -std=c++14 -static \
	  -I vendor/OSMExpress/include \
		-I vendor/OSMExpress/vendor/lmdb/libraries/liblmdb \
		-I vendor/OSMExpress/vendor/libosmium/include \
		-I vendor/OSMExpress/vendor/capnproto/c++/src \
		-I vendor/OSMExpress/vendor/s2geometry/src \
		-I vendor/OSMExpress/vendor/CRoaring/cpp \
		-I vendor/OSMExpress/vendor/CRoaring/include \
		-I ./include \
		-L vendor/OSMExpress/vendor/lmdb/libraries/liblmdb \
		-L vendor/OSMExpress/vendor/capnproto/c++/src/capnp \
		-L vendor/OSMExpress/vendor/capnproto/c++/src/kj \
		-L vendor/OSMExpress/vendor/CRoaring/src \
		./src/main.cpp ./vendor/OSMExpress/src/storage.cpp \
		-llmdb -lcapnp -lkj -lpthread -lroaring \
		-o onramp

osmexpress :
	cd vendor/OSMExpress && \
	cmake -DCMAKE_BUILD_TYPE=Release . && \
	make

clean:
	rm onramp
