FROM fedora:21

MAINTAINER Vivien Malerba "vmalerba@gmail.com"
ENV REFRESHED_AT 2015-09-12

COPY setup-scripts/install.sh setup-scripts/run.sh /
COPY setup-data/ldif-data /ldif-data/
RUN /install.sh

EXPOSE 389

CMD [ "/run.sh" ]