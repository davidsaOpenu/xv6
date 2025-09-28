FROM scratch

ADD test.txt /test.txt
ADD user/sh user/pouch user/ls user/cat user/echo /

# Squash all layers to a single layer
FROM scratch
COPY --from=0 /* /