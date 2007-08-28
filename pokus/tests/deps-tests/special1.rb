#!/usr/bin/ruby


# Check handling of very special provides.


require 'zypptools/lib/package'
require 'zypptools/output/output'
include ZYppTools

require '../../src/testsuite'


packages = Array.new()

pkg = Package.new("test")
pkg.arch = "i586"
pkg.version = "1.2"
pkg.release = 1
pkg.summary = "A pure test package"
pkg.deps :provides, "hello < 2.0"
pkg.deps :provides, "world >= 2.0"
pkg.deps :provides, "modalias(kernel-default:pci:v00008086d00002580sv*sd*bc*sc*i*)"
pkg.deps :provides, "locale(aspell:csb)"
pkg.deps :provides, "perl(Automake::Options) = 0.31"
pkg.deps :provides, "xorg-x11-libs:/usr/X11R6/lib/libXt.so.6"
pkg.deps :provides, "ksym(zt_dtmf_tone) = 150c20a4"
pkg.deps :supplements, "filesystem(xfs)"
packages.push(pkg)


path = Testsuite::write_repo(:yum, packages)


Testsuite.set_arch("i586")
pool = Testsuite.read_repo("file://" + path)


Testsuite::haha2(pool).each do |res|
    puts "#{res.kind_to_s} #{res.name} #{res.edition.to_s} #{res.arch.to_s}"
    puts "  Summary: #{res.summary}"
    Testsuite::dump_all_deps(res)
    puts
end

