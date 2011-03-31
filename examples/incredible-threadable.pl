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
     my $tbb = threads::tbb->new( requires => [ $0 ] );

     my $min = 0;
     my $max = scalar(@{ $self->{input} });
     my $range = threads::tbb::blocked_int->new( $min, $max, 1);

     my $body = $tbb->for_int_method( $self, "my_callback" );

     $body->parallel_for( $range );
 }

 sub my_callback {
     my $self = shift;
     my $int_range = shift;

     for my $idx ($int_range->begin .. $int_range->end-1) {
         my $item = $self->{input}->[$idx];

         my $transmuted = $item->transmogrify;

         $self->{output}->[$idx] = [$transmuted, $threads::tbb::worker||0];
     }
 }

 sub results { @{ $_[0]->{output} } }

 package Item;
 sub transmogrify {
     my $self = shift;
     "Ex-$self->{id}";
 }

 package main;
 use feature 'say';

 unless ($threads::tbb::worker) {  # single script uses can use this
     my $parallel_transmogrificator = Incredible::Threadable->new(
         map { chomp; bless { id => $_ }, "Item" } <>
     );

     $parallel_transmogrificator->parallel_transmogrify();
     my $x = $parallel_transmogrificator;
     say "Turned to '$_->[0]' from ".($_->[1]?"worker $_->[1]":"master thread")
	     for $parallel_transmogrificator->results;
 }
