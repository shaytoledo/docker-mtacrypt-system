FROM ubuntu:22.04
WORKDIR /app
COPY /build/mtaproj-rng /usr/local/bin/mtaproj-rng
COPY /libs/ /usr/local/lib/
RUN chmod +x /usr/local/bin/mtaproj-rng && ldconfig
ENTRYPOINT ["/usr/local/bin/mtaproj-rng"]
