#!/usr/bin/perl -w

use Test::More no_plan;
BEGIN { use_ok('threads::tbb') };

{
	my $init = threads::tbb->new();
	ok($init, "made a perl_tbb_init()");
	isa_ok($init, "threads::tbb", "threads::tbb->new");
}

pass("destroyed perl_tbb_init()");
