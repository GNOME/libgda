FROM fedora:22

MAINTAINER Vivien Malerba "vmalerba@gmail.com"
ENV REFRESHED_AT 2015-09-12

COPY setup-scripts/install.sh /
RUN /install.sh
RUN rm -f /install.sh

# sources files dir
ENV SRC /src

# files installed outside of YUM
ENV DEPEND /dependencies
COPY setup-data/Win32 /dependencies/

# destination of the compiled files
RUN mkdir /install
ENV PREFIX /install

# working dir
RUN mkdir -p /compilation/libgda
WORKDIR /compilation/libgda

COPY setup-scripts/run_configure.sh /compilation/libgda/configure
COPY setup-scripts/mingw-configure /compilation/libgda/.mingw-configure
COPY setup-scripts/do_packages /compilation/libgda/

CMD [ "/bin/bash" ]
