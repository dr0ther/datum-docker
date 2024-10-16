FROM debian:bookworm-slim

RUN apt update

RUN apt install libmicrohttpd12 libjansson4 libsodium23 build-essential cmake curl libmicrohttpd-dev libjansson-dev libcurl4-openssl-dev libpq-dev libgcrypt20-dev libsodium-dev pkg-config -y

WORKDIR /app

COPY datum_gateway /app/

RUN cmake . && make

CMD ["./datum_gateway", "-c datum-setting.json"]