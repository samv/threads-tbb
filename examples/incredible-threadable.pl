# this example is to verify that the example on the man page works.
# It's been modified mostly to print statistics and timing
# information.
#
# The problem is just prepending a string with "Ex-", which is mostly
# serving here as an example of the benefits of the lazy deep copying
# approach: it generally can only help performance.
#
# For example, with threads => 1 and a 1.5MB, 46k line input:
#   loading input: 134ms
#   processing: 330ms
#   Workers processed: 0% :  ()
#   Master processed: 100% : 46332
#   Total processed: 46332
#
# Setting threads => 2 on a 2-way system shows:
#
#   loading input: 134ms
#   processing: 274ms
#   Workers processed: 32.8% : 15203 (1 15203)
#   Master processed: 67.2% : 31129
#   Total processed: 46332
#
# Of course you could argue that the operation is not finished at the
# instrumented points; the lazy deep copying out of the results is
# still yet to happen.  But still, the total wallclock time showed a
# slight decrease with threads => 2, from ~0.74s to ~0.67s - showing
# that even though we started two interpreters, and even though the
# program is completely trivial, some performance improvement can be
# seen at the end of it.  Of course the main thread here was able to
# process data at approximately 2-3 times the speed of the workers.
# For this problem, simply copying the data in and out is the major
# overhead, and so it helps that the master thread can proceed at
# "full steam" for avoiding the overheads exceeding the return.

 package Incredible::Threadable;
 use threads::tbb;

 sub new {
     my $class = shift;
     # make containers which are efficient and thread-safe
     tie my @input, "threads::tbb::concurrent::array";
     push @input, @_;  # coming soon: @input = @_
     tie my @output, "threads::tbb::concurrent::array";
     bless { input => \@input,
             output => \@output, }, $class;
 }

 sub parallel_transmogrify {
     my $self = shift;

     # Initialize the TBB library, and set a specification of required
     # modules and/or library paths for worker threads.
     my $tbb = threads::tbb->new(
	     #threads => 1,
	     requires => [ $0 ] );

     my $min = 0;
     my $max = scalar(@{ $self->{input} });
     my $range = threads::tbb::blocked_int->new( $min, $max, 1 );

     my $body = $tbb->for_int_method( $self, "my_callback" );

     $body->parallel_for( $range );
 }

 sub my_callback {
     my $self = shift;
     my $int_range = shift;

     for my $idx ($int_range->begin .. $int_range->end-1) {
         my $item = $self->{input}->[$idx];

         my $transmuted = $item->transmogrify($threads::tbb::worker);

         $self->{output}->[$idx] = $transmuted;
     }
 }

 sub results { @{ $_[0]->{output} } }

 package Item;
 sub transmogrify {
     my $self = shift;
     my $worker = shift;
     "Ex-$self->{id} ".($worker?"worker $worker":"master");;
 }

 package main;
 use feature 'say';

 use Scriptalicious;
 use List::Util qw(sum);

 unless ($threads::tbb::worker) {  # single script uses can use this
     start_timer();
     my $parallel_transmogrificator = Incredible::Threadable->new(
         map { chomp; bless { id => $_ }, "Item" } <>
     );
     say "loading input: ".show_delta;
     $parallel_transmogrificator->parallel_transmogrify();
     say "processing: ".show_delta;
     my $x = $parallel_transmogrificator;
     my %workers;
     my ($master, $total);
     for ($parallel_transmogrificator->results) {
	     $total++;
	     if ( m{worker (\d+)$} ) {
		     $workers{$1}++;
	     }
	     else {
		     $master++;
	     }
     }
     my $workers_sum = sum values %workers;
     say "Workers processed: ".sprintf("%.1f", ($workers_sum/$total*100))."% : $workers_sum (@{[%workers]})";
     say "Master processed: ".sprintf("%.1f", ($master/$total*100))."% : $master";
     say "Total processed: $total";
 }
