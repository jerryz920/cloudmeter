#!/usr/bin/env ruby


def usage()
  STDERR.print "./sender <host> <port> <size> <s3_state_name> <s3_net_name>"
  exit -1
end

def s3_put key name
  `aws s3 put-object --key #{key} --bucket #{sonarcloud} --body #{name}`
end

def s3_upload key
  #
  #	Note that we upload the entire stat each time
  #
  `tail -n 1 /proc/ec2_netstat >> tmpsenderstat; aws s3 put-object --key #{key} --bucket #{sonarcloud} --body tmpsenderstat`
  sleep 60
end

def start_workload host port size
  `./udp_sender #{host} #{port} #{size}`
end

usage if ARGV.length < 5
Process.daemon(true)

s3_put ARGV[3] "running"

@done = false
t = Thread.new do 
  start_workload ARGV[0] ARGV[1] ARGV[2]
  @done = true
end

s3_upload ARGV[4] while !@done

s3_put ARGV[3] "finished"

