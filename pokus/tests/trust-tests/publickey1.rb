#!/usr/bin/ruby


# Check exception when importing non existing public key.


require 'zypp'
include Zypp


begin
    path = Pathname.new("../../data/not-here.txt")
    publickey = PublicKey.new(path)
    puts publickey.to_s()
rescue ZYppException => e
    puts "ZYppException caught"
    puts e.to_s
else
    puts "Oh, no exception caught"
    exit 1
end

