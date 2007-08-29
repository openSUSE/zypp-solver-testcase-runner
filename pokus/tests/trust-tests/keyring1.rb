#!/usr/bin/ruby


# Check if importing of a public key works.


require 'zypp'
include Zypp


keyring = ZYppFactory::instance.get_zypp.key_ring

path = Pathname.new("../../data/pubkey.txt")
publickey = PublicKey.new(path)
puts publickey.to_s()

id = publickey.id()

puts "is_key_known/trusted #{keyring.is_key_known(id)} #{keyring.is_key_trusted(id)}"

keyring.import_key(publickey, true)

puts "is_key_known/trusted #{keyring.is_key_known(id)} #{keyring.is_key_trusted(id)}"

