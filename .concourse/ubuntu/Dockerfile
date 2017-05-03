FROM ubuntu:latest

COPY pkg.lst /tmp/pkg.lst
RUN apt-get update -y
RUN apt-get install -y $(cat /tmp/pkg.lst)
