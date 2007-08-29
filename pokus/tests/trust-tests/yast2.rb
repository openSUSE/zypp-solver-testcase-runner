#!/usr/bin/ruby


# Generate a YaST repository and delete the public key.


require 'zypptools/lib/arch'
require 'zypptools/lib/package'
require 'zypptools/output/output'
require 'zypp'
include ZYppTools
include Zypp

require '../../src/testsuite'


packages = Array.new()

pkg = Package.new("test")
pkg.arch = "ia64"
pkg.rpmsize = 42
pkg.version = "1.2"
pkg.release = 1
pkg.summary = "A pure test package"
packages.push(pkg)

path = Testsuite.write_repo(:yast, packages)


keyring = ZYppFactory::instance.get_zypp.key_ring
publickey = PublicKey.new(Pathname.new("../../data/pubkey.txt"))
keyring.delete_key(publickey.id(), true)


Testsuite.set_arch("ia64")

begin
    pool = Testsuite.read_repo("file://" + path)
rescue ZYppException => e
    puts "ZYppException caught"
    puts e.to_s
else
    puts "Oh, no exception caught"
    exit 1
end

