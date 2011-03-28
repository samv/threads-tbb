#!/usr/bin/perl
#
#  The previous test did little if anything to test the graph walker;
#  and filled the concurrent arrays only with simple strings.  Here we
#  use an external file to carry out tests.

use Test::More no_plan;
use lib "t";
use threads::tbb;
use strict;

require TestGraphWalker;

my $tbb = threads::tbb->new(threads => 2, modules => [ "TestGraphWalker" ]);

my $test_num = 0;
no strict 'refs';
my $test_func;
use Time::HiRes qw(gettimeofday tv_interval);
use List::Util qw(sum);
while ( defined &{($test_func = "TestGraphWalker::Test".(++$test_num))} ) {
	my ($range, $body, $run_tests) = $test_func->($tbb);
	my @times = times();
	my $now = [ gettimeofday ];
	$tbb->parallel_for($range, $body);
	diag("parallel_for complete") if -t STDOUT;
	my $elapsed = tv_interval $now;
	my $cputime = sum map { $_ - (shift @times) } times();
	diag("cpu: $cputime; elapsed: $elapsed") if -t STDOUT;
	$run_tests->();
	#diag("DONE WITH TESTS");
	if ( $cputime > $elapsed ) {
		pass("cpu time exceeds wallclock");
	}
	elsif ( $elapsed > 0.5 ) {
		diag("elapsed $elapsed, CPU time $cputime: not MP?");
	}
	#diag("ABOUT TO DESTROY BODY / RANGE / ETC");
}

