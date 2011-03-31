 package Incredible::Threadable;
 use threads::tbb;

 sub new {
     my $class = shift;
     # make containers which are efficient and thread-safe
     tie my @input, "threads::tbb::concurrent::array";
     push @input, @_;  # coming soon: @input = @_
     tie my @output, "threads::tbb::concurrent::array";
     bless { input => tied(@input),
             output => tied(@output), }, $class;
 }

 sub parallel_transmogrify {
     my $self = shift;

     # Initialize the TBB library, and set a specification of required
     # modules and/or library paths for worker threads.
     my $tbb = threads::tbb->new( requires => [ $0 ] );

     my $min = 0;
     my $max = $self->{input}->FETCHSIZE;
     my $range = threads::tbb::blocked_int->new( $min, $max, 1);

     my $body = $tbb->for_int_method( $self, "my_callback" );

     $body->parallel_for( $range );
 }

 sub my_callback {
     my $self = shift;
     my $int_range = shift;

     for my $idx ($int_range->begin .. $int_range->end-1) {
         my $item = $self->{input}->FETCH($idx); # lol FIXME :)

         my $transmuted = $item->transmogrify;

         $self->{output}->STORE($idx, [$transmuted, $threads::tbb::worker||0]);
     }
 }

 use feature 'say';
 sub print_results {  # coming soon: no need for this hack!  :)
     my $self = shift;
     my $top = $self->{output}->FETCHSIZE-1;
     for (my $i = 0; $i <= $top; $i++) {
	     my $y = $self->{output}->FETCH($i);
	     say "Item ".($i+1)."/".($top+1).": '$y->[0]' (w$y->[1])";
     }
 }

 package Item;
 sub transmogrify {
     my $self = shift;
     "Ex-$self->{id}";
 }

 package main;

 unless ($threads::tbb::worker) {  # single script uses can use this
     my $parallel_transmogrificator = Incredible::Threadable->new(
         map { chomp; bless { id => $_ }, "Item" } <>
     );

     $parallel_transmogrificator->parallel_transmogrify();
     my $x = $parallel_transmogrificator;
     $parallel_transmogrificator->print_results;
 }
