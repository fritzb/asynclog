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

cmake-build/asynclogTest: cmake-build
	make -C cmake-build asynclogTest


test: cmake-build/asynclogTest
	cd cmake-build && ./asynclogTest

ci: build test
