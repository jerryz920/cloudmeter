#!/usr/bin/env ruby


def usage()
  STDERR.print "./recever <port> <s3_state_name> <s3_net_name>"
  exit -1
end

def s3_put(key,name)
  `aws s3 put-object --key #{key} --bucket sonarcloud --body #{name} `
end

def s3_upload(key)
  #
  #	Note that we upload the entire stat each time
  #
  `cat /proc/ec2_netstat >> /tmp/tmpreceverstat; ifconfig eth0 | grep 'RX bytes' >> /tmp/tmpreceverstat;  aws s3 put-object --key #{key} --bucket sonarcloud --body /tmp/tmpreceverstat`
  sleep 60
end

def start_workload(port)
  `/home/ubuntu/monitor/udp/udp_recever #{port}`
end

usage if ARGV.length < 3

Process.daemon(true)

s3_put(ARGV[1],"running")

@done = false
t = Thread.new do 
  start_workload ARGV[0]
end

s3_upload ARGV[2] while !@done


