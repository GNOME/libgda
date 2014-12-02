FROM fedora:20

MAINTAINER Vivien Malerba "vmalerba@gmail.com"
ENV REFRESHED_AT 2014-11-30

COPY install.sh /
RUN /install.sh
RUN rm -f /install.sh

# sources files dir
ENV SRC /src

# files installed outside of YUM
ENV DEPEND /dependencies
COPY Win64 /dependencies/

# destination of the compiled files
RUN mkdir /install
ENV PREFIX /install

# working dir
RUN mkdir -p /compilation/libgda
WORKDIR /compilation/libgda

COPY run_configure.sh /compilation/libgda/configure
COPY mingw-configure /compilation/libgda/.mingw-configure
COPY do_packages /compilation/libgda/

CMD [ "/bin/bash" ]