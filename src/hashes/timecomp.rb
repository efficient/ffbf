#!/usr/bin/env ruby

require 'benchbase'

infile = ARGV[0]
N = 10
# Warming
system("cat #{infile} > /dev/null")
system("cat #{infile} > /dev/null")

Hashes.each { |hash|
    system("./hash_#{hash} -f #{infile} > /dev/null")
    (0..N).each do
        system("./hash_#{hash} -f #{infile} >> time_#{hash}")
    end
}
