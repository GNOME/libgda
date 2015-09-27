FROM ubuntu:14.04

MAINTAINER Vivien Malerba "vmalerba@gmail.com"
ENV REFRESHED_AT 2015-09-21

ADD setup-scripts/install.sh setup-scripts/run.sh /
ADD setup-data /setup-data
RUN /install.sh

EXPOSE 3050

CMD /run.sh
