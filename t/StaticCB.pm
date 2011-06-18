
package StaticCB;

use Data::Dumper;
use Time::HiRes qw(sleep);

sub myhandler {
	my $range = shift;
	my $array = shift;

	print STDERR "# range: [".$range->begin.",".$range->end.")\n";
	for ( my $i = $range->begin; $i < $range->end; $i++ ) {
		my $item = $array->FETCH($i);
		print STDERR "Got: ".Dumper($item);
		sleep 0.1;
	}
}

sub map_func {
	my $item = shift;
	return( ($item % 7) x (int( ($item+6) / 7)) );
}

1;
