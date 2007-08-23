#!/usr/bin/ruby


# Generate YUM repository containing a package for all architectures.  Check
# for architectures that the correct architectures are loaded into the pool.


require 'zypptools/lib/arch'
require 'zypptools/lib/package'
require 'zypptools/lib/pattern'
require 'zypptools/lib/selection'
require 'zypptools/lib/product'
require 'zypptools/output/output'
include ZYppTools

require '../../src/testsuite'


packages = Array.new()

Arch.all_archs.each do |arch|
    pkg = Package.new("test")
    pkg.arch = arch
    pkg.version = "1.2"
    pkg.release = 1
    pkg.summary = "A pure test package"
    packages.push(pkg)
end


path = Testsuite.write_repo(:yum, packages)


Arch.all_archs.map{|x|x.to_s}.sort.each do |arch|

    x = Arch.new(arch)

    puts "#{x.arch} -> #{x.compat.map{|y|y.to_s}.sort.join(' ')}"

    Testsuite.set_arch(x.arch)

    t = Array.new()

    pool = Testsuite.read_repo("file://" + path)
    Testsuite::haha2(pool).each do |res|
        puts "#{res.kind_to_s} #{res.name} #{res.edition.to_s} #{res.arch.to_s}"
        t.push(res.arch.to_s)
    end

    if not x.compat.map{|y|y.to_s}.sort.eql?(t.sort())
        puts "Error: archs don't match"
        exit 1
    end

    puts

end

