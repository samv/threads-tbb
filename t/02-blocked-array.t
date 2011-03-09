#!/usr/bin/perl -w

use Test::More no_plan;
BEGIN { use_ok('threads::tbb') };

use threads::shared;

use Data::Dumper;
use Scalar::Util qw(reftype refaddr);
my $TERMINAL = ( -t STDOUT );
{
	my @planets :shared;
	@planets = qw(Mercury Venus Earth Mars Jupiter Saturn Uranus
		      Neptune);
	my $range = threads::tbb::blocked_array->new( \@planets, 2 );

	isa_ok($range, "threads::tbb::blocked_array", "range size OK");

	is($range->grainsize, 2, "range size OK");
	is($range->is_divisible, 1, "new blocked_array is divisible");
	is($range->size, 8, "knows its size");

	#my $r2 = $range->split;
	#is($range->size, 4, "split is a mutating constructor");
	#is($r2->size, 4, "returns a new blocked_array");
}
