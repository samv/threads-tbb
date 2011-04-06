
package TestGraphWalker;

use Data::Dumper;

use Time::HiRes qw(sleep);
use constant DEBUG => 0;
BEGIN { if (DEBUG) { require Devel::Peek; Devel::Peek->import; } }


sub doTest {
	my $range = shift;
	my $array = shift;
	my $unwrap = shift;
	my $wrap = shift;

	print STDERR "Processing [".$range->begin.",".$range->end."), worker = ".($threads::tbb::worker ? "YES" : "NO")."\n" if DEBUG;
	$|=1;
	for my $idx ( $range->begin .. $range->end-1 ) {
		my $fetch_item = $array->FETCH($idx);
		print STDERR "Fetched item $idx:\n" if DEBUG;
		Dump($fetch_item) if DEBUG;
		my $num = $unwrap->($array->FETCH($idx));
		my $store_item = $wrap->( $num/3 );
		print STDERR "Storing item $idx:\n" if DEBUG;
		Dump($store_item) if DEBUG;
		$array->STORE($idx, $store_item);
	}
	sleep rand(0.2);
	print STDERR "Done processing [".$range->begin.",".$range->end."), worker = ".($threads::tbb::worker ? "YES" : "NO")."\n" if DEBUG;
}

our $test_num = 1;
sub make_test {
	my $num = $test_num++;
	my $prefix = "Test$num";
	my $wrap = shift;
	my $unwrap = shift;
	my $desc = shift;
	*{"${prefix}Wrap"} = $wrap;
	*{"${prefix}Unwrap"} = $unwrap;
	*{"${prefix}"} = sub {
		TestN( @_, $num, $desc );
	};
	*{"${prefix}Func"} = sub {
		doTest( @_, $unwrap, $wrap );
	};
}

sub TestN {
	my $tbb = shift;
	my $n = shift;
	my $desc = shift || "Test$n";
	my $Wrap = \&{"Test${n}Wrap"};
	my $Unwrap = \&{"Test${n}Unwrap"};

	tie my @vector, "threads::tbb::concurrent::array";

	push @vector, map {
		print STDERR "Storing item ".($_-1).":\n" if DEBUG;
		my $item = $Wrap->($_);
		Dump $item if DEBUG;
		$item;
	} 1..4;

	my $range = threads::tbb::blocked_int->new(0, $#vector+1, 1);
	my $body = $tbb->for_int_array_func(
		tied(@vector), __PACKAGE__."::Test${n}Func",
	);

	return (
		$range,
		$body,
		sub {
			my $i = -1;
			my @all;
			my $pass = 1;
			while ( ++$i <= $#vector ) {
				main::diag("Fetching vector[$i]") if DEBUG;
				my $slot = $vector[$i];
				my $expected_num = ($i+1)/3;
				my $seen_num = $Unwrap->($slot);
				my $expected = $Wrap->($expected_num);
				my $diff = abs($expected_num - $seen_num);
				if ( $diff>0.0001 ) {
					main::diag("[$i] $seen_num ne $expected_num (diff. by $diff)");
					main::diag("slot: ".Dumper($slot));
					main::diag("expected: ".Dumper($expected));
					undef($pass);
					last;
				}
			}
			main::ok($pass, "$desc: all OK");
		}
	);
}

#goto test_this;
make_test sub { "$_[0]" }, sub { 0+$_[0] }, "Test PV";
use Storable qw(freeze thaw);
make_test sub { freeze { foo => 1.0*$_[0] } }, sub { (thaw $_[0])->{foo} }, "Test Storable";
make_test sub { my $x = 1.0*$_[0]; $x }, sub { 1.0*$_[0] }, "Test NV";

make_test sub { \$_[0] }, sub { ${$_[0]} }, "Test REF SCALAR";

make_test sub { [ foo => 1.0*$_[0] ] }, sub { $_[0]->[1] }, "Test AV";
make_test sub { +{ foo => 1.0*$_[0] } }, sub { $_[0]->{foo} }, "Test HV";

# sub FooSV::val {
# 	my $self = shift;
# 	$$self;
# }
# make_test
# 	sub { bless \(1.0*$_[0]), "FooSV" },
# 	sub { $_[0]->val }, "Test Blessed (ref)SV";
# sub FooRV::val {
# 	my $self = shift;
# 	$$$self;
# }
# make_test
# 	sub { bless { \\(1.0*$_[0]) }, "FooRV" },
# 	sub { $_[0]->val }, "Test Blessed (ref)RV";

# sub FooAV::val {
# 	my $self = shift;
# 	$self->[1];
# }
# make_test
# 	sub { bless [ foo => 1.0*$_[0] ], "FooAV" },
# 	sub { $_[0]->val }, "Test Blessed AV";

sub FooHV::val {
	my $self = shift;
	$self->{foo};
}
make_test
	sub { bless { foo => 1.0*$_[0] }, "FooHV" },
	sub { $_[0]->val }, "Test Blessed HV";

sub FooPVMG::val {
	my $self = shift;
	$self->{foo}->FETCH(0);
}
make_test
	sub { tie my @a, "threads::tbb::concurrent::array";
	      push @a, $_[0];
	      bless { foo => tied(@a) }, "FooPVMG" },
	sub { $_[0]->val }, "Test Blessed PVMG";

sub TiedArray::val {
	my $self = shift;
	$self->[1]->[0];
}
make_test
	sub { tie my @a, "threads::tbb::concurrent::array";
	      push @a, $_[0];
	      bless [ tied(@a), \@a ], "TiedArray" },
	sub { $_[0]->val }, "Test Tied Array";


sub SkippedPVMG::val {
	my $self = shift;
	if ( !$threads::tbb::worker or !defined($self->{foo}) ) {
		return $self->{good};
	}
	else {
		return -3;
	}
}
#test_this:
make_test
	sub {
		if ( $threads::tbb::worker ) {
			return bless { good => $_[0] }, "SkippedPVMG";
		}
		else {
			our $tbb ||= threads::tbb->new;
			return bless { (foo => $tbb->{init}), good => $_[0] }, "SkippedPVMG";
		}
	},
	sub { $_[0]->val }, "Test Skipped PVMG";

1;
