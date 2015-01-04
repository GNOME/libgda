FROM fedora:21

MAINTAINER Vivien Malerba "vmalerba@gmail.com"
ENV REFRESHED_AT 2015-01-04

COPY install.sh run.sh /
COPY setup-data /setup-data
RUN /install.sh

EXPOSE 3306

CMD ["/run.sh"]
