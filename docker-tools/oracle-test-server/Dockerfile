FROM ubuntu:14.04

MAINTAINER Vivien Malerba "vmalerba@gmail.com"
ENV REFRESHED_AT 2015-09-14

ADD setup-scripts/install.sh setup-scripts/setup.sh setup-scripts/run.sh /
ADD setup-data /setup-data
RUN /install.sh

EXPOSE 1521
EXPOSE 8080

CMD /run.sh
