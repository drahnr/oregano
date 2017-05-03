FROM fedora:latest

COPY pkg.lst /tmp/pkg.lst
RUN dnf update -y
RUN dnf install -y $(cat /tmp/pkg.lst)
