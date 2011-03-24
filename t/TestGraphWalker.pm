
package TestGraphWalker;

use Data::Dumper;

use Time::HiRes qw(sleep);
#use Devel::Peek;

sub doTest {
	my $body = shift;
	my $range = shift;
	my $unwrap = shift;
	my $wrap = shift;
	my $array = $body->get_array;

	print STDERR "Processing [".$range->begin.",".$range->end."), worker = ".($threads::tbb::worker ? "YES" : "NO")."\n" if -t STDOUT;
	$|=1;
	for my $idx ( $range->begin .. $range->end-1 ) {
		my $fetch_item = $array->FETCH($idx);
		#print STDERR "Fetched item $idx:\n";
		#Dump($fetch_item);
		my $num = $unwrap->($array->FETCH($idx));
		my $store_item = $wrap->( $num/3 );
		#print STDERR "Storing item $idx:\n";
		#Dump($store_item);
		$array->STORE($idx, $store_item);
	}
	sleep rand(0.2);
	print STDERR "Done processing [".$range->begin.",".$range->end."), worker = ".($threads::tbb::worker ? "YES" : "NO")."\n" if -t STDOUT;
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

	push @vector, map { $Wrap->($_) } 1..4;

	my $range = threads::tbb::blocked_int->new(0, $#vector+1, 1);
	my $body = threads::tbb::for_int_func->new(
		$tbb->{init}, __PACKAGE__."::Test${n}Func", tied(@vector),
	);

	return (
		$range,
		$body,
		sub {
			my $i = -1;
			my @all;
			my $pass = 1;
			while ( ++$i <= $#vector ) {
				main::diag("Fetching vector[$i]")
						if -t STDOUT;
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

make_test sub { "$_[0]" }, sub { 0+$_[0] }, "Test PV";
use Storable qw(freeze thaw);
make_test sub { freeze { foo => 1.0*$_[0] } }, sub { (thaw $_[0])->{foo} }, "Test Storable";
make_test sub { my $x = 1.0*$_[0]; $x }, sub { 1.0*$_[0] }, "Test NV";

make_test sub { \$_[0] }, sub { ${$_[0]} }, "Test REF SCALAR";

#make_test sub { +{ foo => 1.0*$_[0] } }, sub { $_[0]->{foo} }, "Test HV";

1;
