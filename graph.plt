# Gnuplot script file for plotting data in file "data.dat"
set autoscale                       # Scale axes automatically
unset log                            # Remove any log-scaling
unset label                          # Remove any previous labels
set title "Sorting Time vs. Number of Threads"
set xlabel "Number of Threads"
set ylabel "Time to Sort (milliseconds)"
set style data linespoints
set term png
set output filename
plot "data.dat" using 2:1 title "Sorting Time" with linespoints
