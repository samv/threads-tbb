#!/usr/bin/perl -w

use strict;
use threads::tbb;

BEGIN {
    unless ( $threads::tbb::worker ) {
    	use Test::More qw(no_plan);
    }
}

# wrap the for_int_method class, which is not designed to support
# being passed between threads.
use threads::tbb::refcounter qw(threads::tbb::for_int_method);
{
    package MyObj;
    use Time::HiRes qw(sleep);
    sub new { my $c = shift; bless {@_}, $c }
    sub callback {
	my $self = shift;
	my $int_range = shift;
	for ( my $i = $int_range->begin; $i < $int_range->end; $i++ ) {
	    $self->{data}->[$i] = $self->{body};
	    sleep 0.05;
	}
    }
}

unless ( $threads::tbb::worker ) {

    ok(defined &threads::tbb::for_int_method::_DESTROY_tbbrc,
       "evidence of setup call working exist");
    is(eval{threads::tbb::for_int_method::CLONE_SKIP()}, 0,
       "class no longer set up to skip on clone");
    ok(defined &threads::tbb::for_int_method::CLONE_REFCNT_inc,
       "CLONE_REFCNT_inc method also defined");

    my $tbb = threads::tbb->new(
	requires => [ $0 ],
	threads => 2,
    );

    my ($body_1, $rc);

    {
	tie my @array, "threads::tbb::concurrent::array";
	push @array, 1..10;
	my $range = threads::tbb::blocked_int->new( 0, 10, 2 );

	my $inv = MyObj->new( data => \@array );

	my $body = $tbb->for_int_method( $inv, "callback" );
	$inv->{body} = $body;

	$body->parallel_for($range);

	for my $i ( 0..9 ) {
	    isa_ok($array[$i], "threads::tbb::for_int_method",
		   "item $i copied back and forth happily");
	}
	$rc = threads::tbb::refcounter::refcount($body);
	cmp_ok($rc, ">", 1,
	       "lots of references to body object");

	$body_1 = $body;
    }
    cmp_ok( threads::tbb::refcounter::refcount($body_1),
	    "<", $rc, "refcount reduced when array destroyed");

    # now, $body_1 should be the only reference to the body object,
    # except for any freed values on the freelists.  Those can be
    # cleared by calling for_int_method again.
    {
	sub static_func { $_[0] * $threads::tbb::worker }
	my @result = $tbb->map_list_func( 'main::static_func', 1..10 );
	ok( (grep { $_ eq 0 } @result), "worker 0 processed some");
	ok( (grep { $_ ne 0 } @result), "worker 1 processed some too");
    };

    no strict 'refs';
    no warnings 'redefine';
    our $called = 0;
    *threads::tbb::for_int_method::_xx =
	\&threads::tbb::for_int_method::_DESTROY_tbbrc;
    *threads::tbb::for_int_method::_DESTROY_tbbrc = sub {
	$main::called++;
	&threads::tbb::for_int_method::_xx(@_);
    };

    # now $body_1 really is the only reference to the body object.
    is(threads::tbb::refcounter::refcount($body_1), 0,
       "no more references to body object");
    is($called, 0, "DESTROY not called yet");
    undef($body_1);
    is($called, 1, "DESTROY called on undef");
}
