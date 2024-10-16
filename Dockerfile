FROM debian:bookworm-slim AS build

RUN apt update

RUN apt install libmicrohttpd12 libjansson4 libsodium23 build-essential cmake curl libmicrohttpd-dev libjansson-dev libcurl4-openssl-dev libpq-dev libgcrypt20-dev libsodium-dev pkg-config -y

WORKDIR /app

COPY datum_gateway /app/

RUN cmake . && make

FROM debian:bookworm-slim AS base

RUN apt update

RUN apt install libmicrohttpd12 libjansson4 libsodium23 curl -y

WORKDIR /app

COPY --from=build /app/datum_gateway /app

LABEL org.opencontainers.image.source https://github.com/retropex/datum-docker

LABEL org.opencontainers.image.description Datum in a container

CMD ["./datum_gateway", "-c datum-setting.json"]
