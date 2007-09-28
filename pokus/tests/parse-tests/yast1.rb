#!/usr/bin/ruby


# Generate a YaST repository and remove the file 'packages'.

# TODO: remove all files one at a time.


require 'zypptools/lib/arch'
require 'zypptools/lib/package'
require 'zypptools/output/output'
include ZYppTools

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

FileUtils.rm(path + "/suse/setup/descr/packages")


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
