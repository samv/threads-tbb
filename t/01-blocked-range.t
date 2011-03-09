#!/usr/bin/perl -w

use Test::More no_plan;
BEGIN { use_ok('threads::tbb') };

use Data::Dumper;

use Scalar::Util qw(reftype refaddr);
my $TERMINAL = ( -t STDOUT );
{
	my $range = threads::tbb::blocked_int->new(1, 10, 1);
	isa_ok($range, "threads::tbb::blocked_int", "t::bb::blocked_int->new");

	is($range->size, 9, "size is 9");

	eval { threads::tbb::blocked_int->new(1, 10); };
	ok($@, "Got an exception");
	diag $@ if $TERMINAL;
}

