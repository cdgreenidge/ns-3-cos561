# Gnuplot script file for plotting data in file "simple-topology-cwnd.data"
set output "simple-topology-cwnd.png"
set   autoscale                        # scale axes automatically
unset log                              # remove any log-scaling
unset label                            # remove any previous labels
set xtic auto                          # set xtics automatically
set ytic auto                          # set ytics automatically
set xlabel "Time (s)"
set ylabel "Congestion Window (bytes)"
set xr [0.0:200.0]
plot    "simple-topology-cwnd.data" using 1:2 title 'TCP New Reno' with lines
