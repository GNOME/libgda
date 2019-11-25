FROM ubuntu:devel

RUN apt-get update -qq && apt-get install --no-install-recommends -qq -y \
	    gcc \
	    gettext \
	    gtk-doc-tools \
	    make \
	    autoconf \
	    meson \
	    ninja-build \
	    libgtk-3-dev \
	    libxml2-dev \
	    gnome-common \
	    libsqlite3-dev \
	    gobject-introspection \
	    libssl-dev \
	    libmysqlclient-dev \
	    libldap2-dev \
	    libpq-dev \
	    libgtksourceview-3.0-dev \
	    libgdk-pixbuf2.0-dev \
	    libgraphviz-dev \
	    libisocodes-dev \
	    libsoup2.4-dev \
	    libxslt1-dev \
	    libjson-glib-dev \
	    libgcrypt20-dev \
	    libssl-dev \
	    libldap2-dev \
	    libgoocanvas-2.0-dev \
	    libhsqldb1.8.0-java \
	    yelp-tools \
	    iso-codes \
	    libgirepository1.0-dev \
	    libgee-0.8-dev \
	    valadoc \
	    libgladeui-dev \
	    postgresql-client \
	    postgresql-client-common \
	    libsqlcipher-dev \
	    python3 \
	    python3-pip \
	    python3-wheel \
	    python3-setuptools \
	    && rm -rf /usr/share/doc/* /usr/share/man/*

ENV LANG=C.UTF-8 LANGUAGE=C.UTF-8 LC_ALL=C.UTF-8

RUN pip3 install meson==0.49.2

ARG HOST_USER_ID=5555
ENV HOST_USER_ID ${HOST_USER_ID}
RUN useradd -u $HOST_USER_ID -ms /bin/bash user

USER user
WORKDIR /home/user

ENV LANG=C.UTF-8 LANGUAGE=C.UTF-8 LC_ALL=C.UTF-8
