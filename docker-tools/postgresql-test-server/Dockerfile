FROM fedora:20

MAINTAINER Vivien Malerba "vmalerba@gmail.com"
ENV REFRESHED_AT 2014-11-20

COPY install.sh run.sh /
COPY setup-data /setup-data
RUN /install.sh

EXPOSE 5432

CMD ["/run.sh"]
