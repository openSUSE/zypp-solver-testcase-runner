#!/usr/bin/ruby


# Generate a YUM repository and remove the file 'primary.xml'.


require 'zypptools/lib/arch'
require 'zypptools/lib/package'
require 'zypptools/output/output'
include ZYppTools

require '../../src/testsuite'


path = "/ARVIN/zypp/trunk/repotools/tmp"


packages = Array.new()

pkg = Package.new("test")
pkg.arch = "ia64"
pkg.rpmsize = 42
pkg.version = "1.2"
pkg.release = 1
pkg.summary = "A pure test package"
packages.push(pkg)

output = OutputFactory(:yum, path)
output.write(packages)

FileUtils.rm(path + "/repodata/primary.xml")


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

