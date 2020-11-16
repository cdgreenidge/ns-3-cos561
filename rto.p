# Gnuplot script file for plotting data in file "simple-topology-throughput.data"
set output "simple-topology-rto.png"
set   autoscale                        # scale axes automatically
unset log                              # remove any log-scaling
unset label                            # remove any previous labels
set xtic auto                          # set xtics automatically
set ytic auto                          # set ytics automatically
set xlabel "Time (s)"
set ylabel "RTO"
plot    "simple-topology-rto.data" using 1:2 title 'TCP New Reno' with lines
