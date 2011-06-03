#!/usr/bin/env ruby

require 'benchbase'

Hashes.each { |hash|
    tn = 0.0
    tt = 0.0
    IO.foreach("time_#{hash}") { |l|
      if (l =~ /^[0-9]/) 
        n, t = l.split.map {|x| x.to_f }
        tn += n
        tt += t
      end
    }
 printf("#{hash} %.2f\n", (tn/tt)/(1024.0*1024.0))
}
