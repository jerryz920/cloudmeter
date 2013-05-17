#!/usr/bin/env ruby

require 'rubygems'
require 'json'


def launch_instance(ami_id, user_data, zone="us-east-1a", region="us-east-1")

  #
  #	Shall we filter out the \" and \' inside user_data? Maybe but not now.
  #
  try_count = 0
  while try_count < 10
    begin
      metastring = `aws ec2 run-instances --image-id #{ami_id} --min-count 1 --max-count 1 --key-name lab --security-group-ids "sg-ae2eddc5" --instance-type "t1.micro"  --user-data "#{user_data}" --region #{region} --placement '{"availability_zone": "#{zone}"}'`
      metahash = JSON.parse(metastring)
      return metahash["Instances"][0]["InstanceId"]
    rescue JSON::ParserError => e
      STDERR.puts "json error! #{e}"
    rescue Exception => e
      STDERR.puts "general error! #{e}"
    end
    try_count = try_count + 1
    sleep 1
  end
  return "-1"
end

def instance_info(instance_id, region="us-east-1")
	try_count = 0
	while try_count < 10
		begin
			metastring = `aws ec2 describe-instances --instance-id #{instance_id} --region #{region}`
			metahash = JSON.parse(metastring)
			return metahash
		rescue JSON::ParserError => e
			STDERR.puts "json error! #{e}"
		rescue Exception => e
			STDERR.puts "general error! #{e}"
		end
		try_count = try_count + 1
		sleep 1
	end
	return {}
end

def instance_public_hostname(metahash)
  metahash["Reservations"].each do |inst|
    return inst["Instances"][0]["PublicDnsName"];
  end
  return ""
end

def instance_private_hostname(metahash)
  metahash["Reservations"].each do |inst|
    return inst["Instances"][0]["PrivateDnsName"];
  end
  return ""
end

def instance_state(metahash)
  metahash["Reservations"].each do |inst|
    return inst["Instances"][0]["State"]["Code"];
  end
  return "80"
end

def terminate_instance(instance_id, region="us-east-1")
	try_count = 0
	while try_count < 20
		`aws ec2 terminate-instances --instance-id #{instance_id} --region #{region}`
		metahash = instance_info(instance_id, region)
		state = instance_state(metahash)
		break if state != 0 and state != 16
		try_count = try_count + 1
	end
	STDERR.puts "intance #{instance_id} is not terminated\n" if try_count >= 20
end
