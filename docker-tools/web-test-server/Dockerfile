FROM fedora:21

MAINTAINER Vivien Malerba "vmalerba@gmail.com"
ENV REFRESHED_AT 2014-11-23

COPY setup-scripts/update-cfg.sh setup-scripts/install.sh setup-scripts/run.sh /
COPY setup-data /setup-data
RUN /install.sh

EXPOSE 80

CMD ["/run.sh"]
