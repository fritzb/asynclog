all:
	docker build . -t fritzb/cmake

push:
	docker push fritzb/cmake

run:
	docker run --rm -ti -e CWD=${PWD} -v /Users:/Users -v ~/.gitconfig:/root/.gitconfig -v ${PWD}:${PWD} fritzb/cmake
