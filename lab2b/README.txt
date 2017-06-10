Lab 2B
Ziheng Yang (Henry)
204584728

Limitations:
In order to best fit the spec, I split the .csv file into two different files -
one for graphs 1-2 and one for graphs 3-5. In addition, I didn't generate more
data points for graph 2 specifically, since all we had to graph was already
generated for graph 1. Splitting the .csv files allows us to avoid having
overlaps in test cases between all 5 graphs.

Included files:
SortedList.h/SortedList.c: same as previous lab, no changes
lab2_add.c: same as previous lab, included in order to generate the first graph
lab2_list.c: added timings for acquiring Mutex as well as option to have
multiple lists in order to test the effect of reduced contention
Makefile: options to build executables, to generate csv files, to make graphs
from said csv files, to create the tarball, and to clean up any generated files
lab_2b_list_1-2.csv/lab_2b_list_3-5.csv: .csv files to generate the graphs,
split into two for graphs 1&2 and graphs 3-5 respectively to avoid overlap of
test cases
profile.gpref: profiled the spinlock test for 1000 iterations and 12 threads as
specified by the LATER part of the spec (as opposed to the beginning which
wanted 32 threads, I did this because of the answer on piazza). Profile includes
the specific breakdown of my thread function
lab2b_1.png: throughput vs threads for mutex/spinlock for add/list
lab2b_2.png: mean time per mutex wait and mean time per operation vs thread
lab2b_3.png: successful runs using the multi-list, identified by number of
iterations vs threads
lab2b_4.png: throughput vs threads for mutex for multi-list
lab2b_5.png: throughput vs threads for spinlock for multi-list

2.3.1

For single and double threaded proccesses, most of the cycles are spent on doing
productive work, such as updating the counter for add and making changes to the
linkedlist for list (insertion, lookup, delete). This is because the low thread
level results in few cycles spent on lock contention.

In the high-thread spinlock tests, most of the cycles is spent waiting for the
spinlock. Specifically, each thread not holding the lock does nothing productive
with its time, unyielding until its timeslice is over.

In the high-thread mutex tests, most of the cycles is spent performing
productive operations. This is because when a thread does not have the lock
under mutex, the OS blocks the thread and puts it to sleep so that no cycles are
wasted.

2.3.2

In the high thread environment highlighted by the profile.gpref file generated
with gperftools, the lines that eat up most of the cycles is the line that
attempts to acquire the lock. Specifically, the while loop that does "test and
set."

This operation becomes expensive with a large amount of threads because threads
without the lock are forced to keep trying to acquire the lock using the
spinlock algorithm. This is especially expensive with a large number of threads
as contention increases with more threads. The threads that do not have the lock
essentially spin and wait for the entire duration of their timeslice,
contributing no productive work and is consequently extremely expensive.

2.3.3

Average lock-wait time increases dramatically with an increased number of
threads because contention increases with more threads. Specifically, each
thread has to wait longer for its turn since there are more threads fighting for
the same lock.

Completion time increases less dramatically because it is largely focused on the
actual operations, which is less pertinent to contention.

Wait time increases more dramatically than completion time because wait time is
significantly affected by the number of threads (by contention), whereas
completion time is just how long it takes for the operation to finish, and often
times the operation can finish without giving up the lock.

2.3.4

The throughput appears to increase as a function of the number of lists.

It should continue to increase towards an asymptote - the savings in contention 
will drop off significantly until we have as many lists as we have elements.

This question makes no sense because 1/N threads is nonsense (fractional
threads?). My graph seems to suggest that there is some sort of 
relationship between number of threads and number of lists - specifically, if we
have a large number of lists and a large number of threads it performs similarly
to a small number of lists and a small number of threads. This is because
contention is increased with more threads but decreased with more lists.
