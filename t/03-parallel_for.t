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

my $init = threads::tbb->new(4);

my $range = threads::tbb::blocked_int->new(0, scalar(@array), 2);
is($range->end, 10, "Made a blocked range");

my $body = threads::tbb::map_int_body->new("bob");
isa_ok($body, "threads::tbb::map_int_body", "new map_int_body");

parallel_for($range, $body);
pass("didn't crash!");
