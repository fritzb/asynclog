FROM alpine:3.9
RUN apk update && \
  apk add git make cmake g++ gdb valgrind vim bash
CMD /bin/bash -c "cd $CWD; bash"
