#!/usr/bin/env ruby


require './ec2'
require './s3'
require './config'


netstat_tests = [
  {
  	:desc => "internet inbound test",
	:name => "int_in", 
	:ami_id  => COMMON_ARGS[:ami_id],
	:recever => :new_ami_recever,
	:recever_ip => :public_host,
	:recever_port => COMMON_ARGS[:recever_port],
	:recever_region => "us-east-1",
	:recever_zone => "us-east-1a",
	:sender_region => "us-east-1",
	:sender_zone => "us-east-1a",
	:sender => :new_local_sender,
  } ,
  {
  	:desc => "internet outbound test",
	:name => "int_out", 
	:ami_id  => COMMON_ARGS[:ami_id],
	:recever => :new_local_recever , 
	:recever_ip => "public_host",
	:recever_port => COMMON_ARGS[:recever_port],
	:recever_region => "us-east-1",
	:recever_zone => "us-east-1a",
	:sender_region => "us-east-1",
	:sender_zone => "us-east-1a",
	:sender => :new_ami_sender,
  },
  {
  	:desc => "ec2 private IP test",
	:name => "ec2_pri", 
	:ami_id  => COMMON_ARGS[:ami_id],
	:recever => :new_ami_recever,
	:recever_ip => "private_host",
	:recever_port => COMMON_ARGS[:recever_port],
	:recever_region => "us-east-1",
	:recever_zone => "us-east-1a",
	:sender_region => "us-east-1",
	:sender_zone => "us-east-1a",
	:sender => :new_ami_sender,
  },
  {
  	:desc => "ec2 public IP test",
	:name => "ec2_pub", 
	:ami_id  => COMMON_ARGS[:ami_id],
	:recever => :new_ami_recever,
	:recever_ip => "public_host",
	:recever_port => COMMON_ARGS[:recever_port],
	:recever_region => "us-east-1",
	:recever_zone => "us-east-1a",
	:sender_region => "us-east-1",
	:sender_zone => "us-east-1a",
	:sender => :new_ami_sender,
  },
  {
  	:desc => "ec2 zone test",
	:name => "ec2_zone",
	:ami_id  => COMMON_ARGS[:ami_id],
	:recever => :new_ami_recever,
	:recever_ip => "public_host",
	:recever_port => COMMON_ARGS[:recever_port],
	:recever_region => "us-east-1",
	:recever_zone => "us-east-1a",
	:sender_region => "us-east-1",
	:sender_zone => "us-east-1b",
	:sender => :new_ami_sender,
  },
  {
  	:desc => "ec2 region test",
	:name => "ec2_region",
	:ami_id  => COMMON_ARGS[:ami_id],
	:recever => :new_ami_recever,
	:recever_ip => "public_host",
	:recever_port => COMMON_ARGS[:recever_port],
	:recever_region => "us-east-1",
	:recever_zone => "us-east-1a",
	:sender_region => "us-west-1",
	:sender_zone => "us-east-1a",
	:sender => :new_ami_sender,
  },
]

class Worker
  def launch(id, suffix, arghash)
  end

  def status()
  end

  def public_host
    return COMMON_ARGS[:localhost]
  end

  def private_host
    return COMMON_ARGS[:localhost]
  end

  def ready?
    return true
  end

  def finished?
    return true
  end

  def terminate()
  end

  def show_result(header)
  end
end


class AMIWorker < Worker
  attr_accessor :instance_id
  attr_accessor :suffix
  attr_accessor :state_key
  attr_accessor :acct_key

  def launch(id, suffix, arghash)
	@region = arghash[:REGION]
	@zone = arghash[:ZONE]
  end

  def public_host
    fetch_ip if !@public_host || !@private_host
    @public_host
  end

  def private_host
    fetch_ip if !@public_host || !@private_host
    @private_host
  end

  def status
    metahash = instance_info(@instance_id, @region)
    return instance_state(metahash)
  end

  def fetch_ip
    metahash = instance_info(@instance_id, @region)
    @public_host = instance_public_hostname(metahash)
    @private_host = instance_private_hostname(metahash)
  end

  def show_result(header)
    tmpfilename="/tmp/s3acct.#{Time.now.to_i}.#{Process.pid}"
    puts header
    s3_get_object(@acct_key, tmpfilename)
    puts `tail -n 2 #{tmpfilename} | ./parser`
    File.delete(tmpfilename)
  end

  def terminate()
    terminate_instance(@instance_id)
  end
end

class AMIRecever < AMIWorker
  def launch(id, suffix, arghash)
    	super id, suffix, arghash
	@state_key = "recever_state.#{suffix}"
	@acct_key = "recever_acct.#{suffix}"
	attr_string = "export RECEVER_PORT=#{arghash[:PORT]};"
	attr_string << "export CLOUDMETER_TYPE=recever;"
	attr_string << "export STATE_KEY=#{@state_key};"
	attr_string << "export ACCT_KEY=#{@acct_key};"
	puts "#{attr_string}"
	@instance_id = launch_instance(id, attr_string, @zone, @region)
  end

  def ready?
    tmpfilename="/tmp/s3state.#{Time.now.to_i}.#{Process.pid}"
    s3_get_object(@state_key, tmpfilename)
    content = File.read tmpfilename
    File.delete(tmpfilename)
    puts "content=[#{content}]"
    if content =~ /running/ then true else false end
  end
end

class AMISender < AMIWorker
  def launch(id, suffix, arghash)
    	super id, suffix, arghash
	@state_key = "sender_state.#{suffix}"
	@acct_key = "sender_acct.#{suffix}"
	attr_string = "export RECEVER_PORT=#{arghash[:PORT]};"
	attr_string << "export RECEVER_HOST=#{arghash[:HOST]};"
	attr_string << "export RECEVER_SIZE=#{arghash[:SIZE]};"
	attr_string << "export CLOUDMETER_TYPE=sender;"
	attr_string << "export STATE_KEY=#{@state_key};"
	attr_string << "export ACCT_KEY=#{@acct_key}"
	@instance_id = launch_instance(id, attr_string, @zone, @region)
  end

  def finished?
    tmpfilename = "/tmp/s3obj.#{Time.now.to_i}.#{Process.pid}"
    s3_get_object(@state_key, tmpfilename)
    content = File.read tmpfilename
    File.delete(tmpfilename)
    if content =~ /finished/ then true else false end
  end
end

class LocalSender < Worker
  attr_accessor :pid
  def launch(path, suffix, arghash)
    @pid = Process.fork do
      Process.exec("#{path} #{arghash[:HOST]} #{arghash[:PORT]} #{arghash[:SIZE]}")
    end
  end

  def status
    return "1"
  end

  def finished?
    Process.wait(@pid)
    true
  end

end

class LocalRecever < Worker
  attr_accessor :pid
  def launch(path, suffix, arghash)
    @pid = Process.fork do
    	Process.exec("#{path} #{arghash[:PORT]}")
    end
  end

  def status
    return "1"
  end

  def terminate()
    Process.kill("KILL", @pid)
    Process.wait(@pid)
  end
end


def new_ami_recever(args)
  recever = AMIRecever.new
  recever.launch(args[:ami_id], "#{args[:name]}.#{Time.now.to_i}", {
    :PORT => args[:recever_port],
    :ZONE => args[:recever_zone],
    :REGION => args[:recever_region],
  })
  return recever
end

def new_ami_sender(args, recever)
  sender = AMISender.new
  sender.launch(args[:ami_id], "#{args[:name]}.#{Time.now.to_i}", {
    :PORT => args[:recever_port],
    :HOST => recever.__send__(args[:recever_ip]),
    :SIZE => COMMON_ARGS[:sendsize],
    :ZONE => args[:sender_zone],
    :REGION => args[:sender_region],
  })
  return sender
end

def new_local_recever(args)
  recever = LocalRecever.new
  recever.launch("./udp_recever", "#{args[:name]}.#{Time.now.to_i}", {
    :PORT => args[:recever_port],
  })
  return recever

end

def new_local_sender(args, recever)
  sender = LocalSender.new
  sender.launch("./udp_sender", "#{args[:name]}.#{Time.now.to_i}", {
    :PORT => args[:recever_port],
    :HOST => recever.__send__(args[:recever_ip]),
    :SIZE => COMMON_ARGS[:sendsize]
  })
  return sender
end

#
#
#	Control script
#
#
test_no = if ARGV.length > 0 then ARGV[0].to_i else 0 end ;

recever = __send__(netstat_tests[test_no][:recever], netstat_tests[test_no])

#
# wait 3 minutes to setup recever
#
try_count = 0
while (not recever.ready?) && try_count < 36
  sleep 5
  try_count = try_count + 1
end

print "recever launched!\n"

sender = __send__(netstat_tests[test_no][:sender], netstat_tests[test_no], recever)

while not sender.finished?
  sleep 5
end

sender.show_result("sender status");
recever.show_result("recever status");

sender.terminate
recever.terminate




