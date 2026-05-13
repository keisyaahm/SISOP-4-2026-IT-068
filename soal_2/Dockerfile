FROM ubuntu:latest

WORKDIR /app

COPY server /app/server

RUN mkdir -p /app/db

RUN chmod +x /app/server

EXPOSE 9000

CMD ["./server"]
