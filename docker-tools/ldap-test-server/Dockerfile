FROM fedora:21

MAINTAINER Vivien Malerba "vmalerba@gmail.com"
ENV REFRESHED_AT 2014-12-13

COPY install.sh run.sh /
COPY ldif-data /ldif-data/
RUN /install.sh

EXPOSE 389

CMD [ "/run.sh" ]