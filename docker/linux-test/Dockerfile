FROM ubuntu:18.04

## for apt to be noninteractive
ENV DEBIAN_FRONTEND noninteractive 
ENV DEBCONF_NONINTERACTIVE_SEEN true
ENV CFLAGS -g -O0

RUN echo "root:Docker!" | chpasswd

RUN apt-get update && \
    apt-get -y dist-upgrade

# pre-seed tzdata, update package index, upgrade packages and install needed
# software
RUN echo "tzdata tzdata/Areas select Europe" > /tmp/preseed.txt; \
    echo "tzdata tzdata/Zones/Europe select London" >> /tmp/preseed.txt; \
    debconf-set-selections /tmp/preseed.txt && \
    rm -f /etc/timezone && \
    rm -f /etc/localtime && \
    apt-get install -y tzdata

# https://github.com/pyenv/pyenv/wiki/Common-build-problems
RUN apt-get -y install build-essential \
                       curl \
                       gdb \
                       sudo \
                       python-dev \
                       python3-dev \
                       libffi-dev \
                       ca-cacert \
                       libncurses5-dev libncursesw5-dev \
                       git \
                       ruby-dev \
                       lua5.2 \
                       liblua5.2-dev \
                       lua5.3 \
                       liblua5.3-dev \
                       tcl-dev \
                       tcllib \
                       libssl-dev\
                       zlib1g-dev\
                       libbz2-dev\
                       libreadline-dev\
                       libsqlite3-dev\
                       wget\
                       curl\
                       man\
                       llvm\
                       libncurses5-dev\
                       libncursesw5-dev\
                       xz-utils\
                       tk-dev\
                       libffi-dev\
                       liblzma-dev\
                       ctags \
                       python-openssl && \
    apt-get -y autoremove

## cleanup of files from setup
RUN rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*

RUN useradd -ms /bin/bash -d /home/dev -G sudo dev && \
    echo "dev:dev" | chpasswd && \
    echo '%sudo ALL=(ALL) NOPASSWD:ALL' >> /etc/sudoers.d/sudo

USER dev
WORKDIR /home/dev

ENV HOME /home/dev
ENV PYTHON_CONFIGURE_OPTS --enable-shared

ENV TERM screen-256color

ENV CONF_ARGS "--enable-rubyinterp=dynamic  \
               --enable-tclinterp \
               --enable-multibyte  \
               --enable-cscope \
               --enable-python3interp \
               --enable-pythoninterp \
               --enable-tclinterp \
               --enable-luainterp \
               --with-features=huge \
               --enable-terminal \
               --enable-fail-if-missing"
