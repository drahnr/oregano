FROM fedora:latest
ENV DRONE_SERVER_PORT :80

RUN yum install -y dnf
RUN dnf update
RUN dnf install -y gtk3-devel libxml2-devel gtksourceview3-devel intltool glib2-devel goocanvas2-devel rpmdevtools

COPY id_rsa ~/.ssh/
COPY id_rsa.pub ~/.ssh/
