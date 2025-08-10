FROM ubuntu:22.04
WORKDIR /app
COPY /build/mtaproj-decrypter /usr/local/bin/mtaproj-decrypter
COPY /libs/ /usr/local/lib/
RUN chmod +x /usr/local/bin/mtaproj-decrypter && ldconfig
ENTRYPOINT ["/usr/local/bin/mtaproj-decrypter"]
