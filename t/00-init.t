#!/usr/bin/perl -w

use Test::More no_plan;
BEGIN { use_ok('threads::tbb') };

use Data::Dumper;

use Scalar::Util qw(reftype refaddr);
my $TERMINAL = ( -t STDOUT );
{
	my $init = threads::tbb::init->new();
	ok($init, "made a perl_tbb_init()");
	isa_ok($init, "threads::tbb::init", "threads::tbb::init->new");
	diag "ref: ".ref $init, " - ".reftype($init).sprintf("(0x%x)", refaddr($init)) if $TERMINAL;
	diag Dumper $init if $TERMINAL;

	$init->initialize(4);
	pass("called initialize");

	$init->terminate;
	pass("called terminate");
}

pass("destroyed perl_tbb_init()");
