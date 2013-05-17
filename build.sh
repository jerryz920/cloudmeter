#!/bin/bash

sudo pip install awscli

cd control
rm udp_server udp_recever parser
make udp_sender
make udp_recever
make parser
cd ..
