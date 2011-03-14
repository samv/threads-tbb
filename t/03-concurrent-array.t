#!/usr/bin/perl

use Test::More no_plan;
use strict;

BEGIN { use_ok("threads::tbb") }

my $array_tie_obj = threads::tbb::concurrent::array->new;

isa_ok($array_tie_obj, "threads::tbb::concurrent::array", "new perl_concurrent_vector");
is($array_tie_obj->size, 0, "Knows its size");

tie my @array, "threads::tbb::concurrent::array";

isa_ok(tied(@array), "threads::tbb::concurrent::array", "tied(\@array)");
is( scalar(@array), 0, "FETCHSIZE()" );
