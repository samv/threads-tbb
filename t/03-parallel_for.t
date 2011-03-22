#!/usr/bin/perl
#
#  first test of actual parallelisation
#

use Test::More no_plan;
use strict;
BEGIN { use_ok("threads::tbb") };

tie my @array, "threads::tbb::concurrent::array";
pass("made an array");
push @array, qw(Parker Lady_Penelope Brains Virgil_Tracy Jeff_Tracey
		John_Tracy Kyrano The_Hood Tin_Tin Alan_Tracy);
is(@array, 10, "put 10 strings in it");

threads::tbb::set_superglobal(tied @array);

my $tbb = threads::tbb->new(
	threads => 4,
	modules => [],
);

my $range = threads::tbb::blocked_int->new(0, scalar(@array), 2);
is($range->end, 10, "Made a blocked range");

my $body = threads::tbb::map_int_body->new(
	$tbb->{init}, "threads::tbb::map_int_body::somemethod",
);
isa_ok($body, "threads::tbb::map_int_body", "new map_int_body");

parallel_for($tbb->{init}, $range, $body);
pass("didn't crash!");
