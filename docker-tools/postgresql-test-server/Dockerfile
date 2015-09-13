FROM fedora:21

MAINTAINER Vivien Malerba "vmalerba@gmail.com"
ENV REFRESHED_AT 2015-09-12

COPY setup-scripts/install.sh setup-scripts/run.sh /
COPY setup-data /setup-data
RUN /install.sh

EXPOSE 5432

CMD ["/run.sh"]
