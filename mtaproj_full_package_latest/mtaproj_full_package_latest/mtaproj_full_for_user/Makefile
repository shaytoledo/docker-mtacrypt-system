HUB          ?= omerovadia1
PROJECT      ?= mtaproj
RNG_NAME     ?= $(PROJECT)-rng
DEC_NAME     ?= $(PROJECT)-decrypter
TAGS         ?= latest

RNG_DOCKERFILE ?= docker/rng/Dockerfile
DEC_DOCKERFILE ?= docker/decrypter/Dockerfile
BUILD_ARGS   ?= --platform linux/amd64

.PHONY: all build push rng rng-push dec dec-push login clean verify

all: build push

login:
	@docker login -u $(HUB)

build: rng dec

rng:
	@for tag in $(TAGS); do \
	  docker build $(BUILD_ARGS) -f $(RNG_DOCKERFILE) -t $(HUB)/$(RNG_NAME):$$tag . || exit 1; \
	done

dec:
	@for tag in $(TAGS); do \
	  docker build $(BUILD_ARGS) -f $(DEC_DOCKERFILE) -t $(HUB)/$(DEC_NAME):$$tag . || exit 1; \
	done

push: rng-push dec-push

rng-push:
	@for tag in $(TAGS); do docker push $(HUB)/$(RNG_NAME):$$tag || exit 1; done

dec-push:
	@for tag in $(TAGS); do docker push $(HUB)/$(DEC_NAME):$$tag || exit 1; done

verify:
	@docker pull $(HUB)/$(RNG_NAME):latest
	@docker pull $(HUB)/$(DEC_NAME):latest
	@echo "RNG run (3s)..."; timeout 3 docker run --rm $(HUB)/$(RNG_NAME):latest || true
	@echo "Decrypter echo test..."; echo "hello" | docker run -i --rm $(HUB)/$(DEC_NAME):latest

clean:
	-@for tag in $(TAGS); do \
	  docker rmi $(HUB)/$(RNG_NAME):$$tag 2>/dev/null || true; \
	  docker rmi $(HUB)/$(DEC_NAME):$$tag 2>/dev/null || true; \
	done
