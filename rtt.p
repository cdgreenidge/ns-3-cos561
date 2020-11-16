# Gnuplot script file for plotting data in file "simple-topology-rtt.data"
set output "simple-topology-rtt.png"
set   autoscale                        # scale axes automatically
unset log                              # remove any log-scaling
unset label                            # remove any previous labels
set xtic auto                          # set xtics automatically
set ytic auto                          # set ytics automatically
set xlabel "Time (s)"
set ylabel "RTT (s)"
plot    "simple-topology-rtt.data" using 1:2 title 'TCP New Reno' with lines
