#!/usr/bin/env ruby

def s3_get_object(key, filename)
  `aws s3 get-object --bucket sonarcloud --key #{key} #{filename}`
end


