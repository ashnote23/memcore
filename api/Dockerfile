# Stage 1 - Build
FROM golang:1.25 AS builder
WORKDIR /app
COPY go.mod go.sum ./
RUN go mod download
COPY . .
RUN go build -o api .

# Stage 2 - Run
FROM debian:bookworm-slim
WORKDIR /app
COPY --from=builder /app/api .
EXPOSE 8080
CMD ["./api"]