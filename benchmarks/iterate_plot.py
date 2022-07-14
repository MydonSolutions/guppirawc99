import re
import matplotlib.pyplot as plt

time_ratio = []
chan_ratio = []
throughput = []

line_regex = r'.*?time=(\d+/\d+), chan=(\d+/\d+), aspect=(\d+/\d+)\.\.\..*?\((\d+\.\d+) GB/s\).*'
with open('./iterate.txt', 'r') as fio:
  for line in fio:
    line_match = re.match(line_regex, line)
    if line_match is not None:
      aspect_ratio = eval(line_match.group(3))
      if aspect_ratio == 1:
        time_ratio.append(eval(line_match.group(1)))
        chan_ratio.append(eval(line_match.group(2)))
        throughput.append(eval(line_match.group(4)))

# Creating figure
fig = plt.figure(figsize =(12, 9))
ax = plt.axes(projection ='3d')
ax.view_init(50, -50)

cmap = plt.get_cmap('hot')
 
# Creating plot
trisurf = ax.plot_trisurf(
  time_ratio, chan_ratio, throughput,
  cmap = cmap,
  linewidth = 0.2, antialiased = True
)
fig.colorbar(trisurf, ax = ax, shrink = 0.5, aspect = 5)
ax.set_title('Iteration Throughput for Different Time/Channel Steps (All aspects)')
 
# Adding labels
ax.set_xlabel('Time Ratio (time_iter/NTIME)', fontweight ='bold')
ax.set_ylabel('Channel Ratio (chan_iter/OBSNCHAN)', fontweight ='bold')
ax.set_zlabel('Throughput (GB/s)', fontweight ='bold')

plt.savefig(f"iterate_benchmark.png")
plt.clf()