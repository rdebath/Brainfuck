print ARGF.read.split('').map{|i|'+'*(i.ord/10)+'[>'+'+'*10+'<-]>'+'+'*(i.ord%10)+".[-]<\n"}.join
