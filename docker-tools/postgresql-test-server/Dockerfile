FROM fedora:21

MAINTAINER Vivien Malerba "vmalerba@gmail.com"
ENV REFRESHED_AT 2014-12-13

COPY install.sh run.sh /
COPY setup-data /setup-data
RUN /install.sh

EXPOSE 5432

CMD ["/run.sh"]
