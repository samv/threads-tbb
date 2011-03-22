
package StaticFunc;

use Data::Dumper;
use Time::HiRes qw(sleep);

sub myhandler {
	my $self = shift;
	my $range = shift;
	my $array = $self->get_array;

	print STDERR "# range: [".$range->begin.",".$range->end.")\n";
	for ( my $i = $range->begin; $i < $range->end; $i++ ) {
		my $item = $array->FETCH($i);
		print STDERR "Got: ".Dumper($item);
		sleep 0.1;
	}
}

1;
