#!/usr/bin/ruby


# Specify a broken URL.


require '../../src/testsuite'


Testsuite.set_arch("x86_64")

begin
    pool = Testsuite.read_repo("htp://site/repo/")
rescue ZYppException => e
    puts "ZYppException caught"
    puts e.to_s
else
    puts "Oh, no exception caught"
    exit 1
end

