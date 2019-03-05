build: cmake-build
	cd cmake-build && make

start:
	docker run --rm -ti -e CWD=${PWD} -v /Users:/Users -v ~/.gitconfig:/root/.gitconfig -v ${PWD}:${PWD} fritzb/cmake

cmake-build:
	mkdir cmake-build
	cd cmake-build && cmake ..

clean:
	-@rm -fR cmake-build


cmake:
	make -C docker all
	make -C docker push

cmake-build/memlogTest: cmake-build
	make -C cmake-build memlogTest


test: cmake-build/memlogTest
	cd cmake-build && ./memlogTest

ci: build test
