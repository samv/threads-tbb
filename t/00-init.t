#!/usr/bin/perl -w

use Test::More tests => 3;
BEGIN { use_ok('threads::tbb') };

{
	my $init = threads::tbb->new();
	ok($init, "made a perl_tbb_init()");
}

ok("destroyed perl_tbb_init()");
