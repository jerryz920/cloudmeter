#!/bin/bash

#export AWS_ACCESS_KEY_ID=AKIAIURU4SIO3ITY7CIA
#export AWS_SECRET_ACCESS_KEY=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
#
#	Fetch metadata, if ${CLOUDMETER_TYPE} == sender",
#	Then we have ${RECEVER_HOST} ${RECEVER_PORT} ${RECEVER_SIZE}
#	if ${CLOUDMETER_TYPE == recever, then we have ${RECVER_PORT}
#	each case we have a STATE_KEY and ACCT_KEY speicifed
#
cd /var/run/cloudmeter
echo 'running' > running
echo 'finished' > finished
. /etc/profile
#export AWS_CONFIG_FILE=/home/ubuntu/.aws

wget http://169.254.169.254/latest/user-data -O userenv.sh

if [ -f userenv.sh ]; then
	. userenv.sh
else
	export CLOUDMETER_TYPE=recever
	export RECEVER_PORT=1985
	export STATE_KEY=recever_state
	export ACCT_KEY=recever_acct
	export AWS_ACCESS_KEY_ID=AKIAIURU4SIO3ITY7CIA
	export AWS_SECRET_ACCESS_KEY=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
fi 

case $CLOUDMETER_TYPE in
	sender) /usr/bin/ruby1.9.3 /home/ubuntu/monitor/s3/sender.rb ${RECEVER_HOST} ${RECEVER_PORT} ${RECEVER_SIZE} ${STATE_KEY} ${ACCT_KEY}
;;
	*) /usr/bin/ruby1.9.3 /home/ubuntu/monitor/s3/recever.rb ${RECEVER_PORT:-1985} ${STATE_KEY:-recever_state} ${ACCT_KEY:-recever_acct}
;;
esac
