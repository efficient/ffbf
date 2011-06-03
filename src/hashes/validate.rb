#!/usr/bin/env ruby

require 'benchbase'
a = Array.new
LEN=19

ARGF.each_line { |l|
    a << l.chomp.reverse
}

tmp = File.new("tmp", "w")
tmp.puts a.sort.map { |l| l.reverse }
tmp.close

Hashes.each { |hash|
    puts "Checking #{hash}"
    IO.popen("./hash_#{hash} tmp") { |pipe|
        prev = ""
        prev_hash = ""
    
        pipe.each_line { |l|
            hash, word = l.chomp.split($;, 2)
            if (word.length > LEN)
                wtrim = word[-LEN..-1]
            else
                wtrim = word
            end
            if (wtrim == prev)
                if (prev_hash != hash)
                    puts "Hash mismatch: #{l} #{wtrim} - #{prev_hash} #{hash}"
                end
            end
            
            prev = wtrim.dup
            prev_hash = hash
        }
    }
}
