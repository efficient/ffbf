#!/usr/bin/env ruby

require 'benchbase'
require 'optparse'
require 'fileutils'
include FileUtils

usehash = nil
options = {}
OptionParser.new do |opts|
    opts.banner = "Usage: distcomp.rb [-H hash] <infile>"
    opts.on("-H", "--hash HASH", "specify hash") do |h|
        usehash = h
    end
    opts.on_tail("-h", "--help", "Show this message") do
        puts opts
        exit
    end
end.parse!

infile = ARGV[0]
LEN=19
system("cut -c 1-#{LEN} #{infile} | sort -u > tmp")
linecount = 0
IO.foreach("tmp") { linecount += 1 }

puts "# distcomp.rb " + Time::new.to_s
puts "# user: #{ENV["USER"]}"
puts "# input file: #{infile}"
puts "# lines: #{linecount}"
puts "#"
puts "# hashname  collisions"

hashes = usehash ? [ usehash ] : Hashes

hashes.each { |hash|
    IO.popen("./hash_#{hash} tmp") { |pipe|
        count = Hash.new
        pipe.each_line { |l|
            hval, word = l.chomp.split($;, 2)
            
            count[hval] ||= 0
            count[hval] += 1
        }
        colls = 0
        count.each_pair { |k, v|
            if (v > 1)
                colls += v-1;
            end
        }
        puts "#{hash} #{colls}"
#    shashes = count.keys.sort_by { |x| count[x] }.reverse
#    shashes.each { |h|
#        if (count[h] <= 1)
#            break
#        end
#        colls += count[h]
#        puts "#{h} #{count[h]}"
#    }
    }
}
