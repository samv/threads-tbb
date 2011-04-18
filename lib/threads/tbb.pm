package threads::tbb;

use 5.008;
use strict;
use warnings;
use Carp;

our $VERSION = '0.00_03';

require XSLoader;
XSLoader::load('threads::tbb', $VERSION);

# this contains paths given to child perls as -Mlib=xxx
our @BOOT_LIB;

# these variables are filled with defaults for 'use' in child perls
our %BOOT_INC;
our @BOOT_USE;

our $worker;  # set in XS code if we're a worker thread

BEGIN {
	# save what .pm files are already included - these need to be
	# loaded in children.
	@BOOT_LIB = @INC;
	%BOOT_INC = %INC;

}

sub track_use {
	shift;
	my $filename = shift;
	unless ( $BOOT_INC{$filename}
			 || (grep { $_ eq $filename } @BOOT_USE)
		 ) {
		push @BOOT_USE, $filename;
	}
}

sub new {
	my $class = shift;
	my $self = bless {}, $class;
	my %options;
	if ( @_ == 1 ) {
		$options{threads} = shift;
	}
	elsif ( @_ % 2 ) {
		croak "odd number of arguments passed";
	}
	else {
		%options = @_;
	}
	$self->create_task_scheduler_init(\%options);
	$self->setup_task_scheduler_init(\%options);
	$self->initialize(\%options);
	return $self;
}

sub terminate {
	my $self = shift;
	$self->{init}->terminate;
}

sub create_task_scheduler_init {
	my $self = shift;
	my $options = shift;
	$self->{init} = threads::tbb::init->new( $options->{threads} || -2 );
}

sub default_boot_use {
	[ (sort { ($a =~ tr{/}{/}) <=> ($b =~ tr{/}{/}) or $a cmp $b }
		   keys %BOOT_INC),
	  @BOOT_USE ];
}
sub default_boot_lib { \@BOOT_LIB }

sub setup_task_scheduler_init {
	my $self = shift;
	my $options = shift;

	if ( $options->{modules} ) {
		# specifying modules overrides the automatically
		# collected stuff
		$self->{init}->set_boot_use( [
			map { my $x = $_; $x =~ s{::}{/}g; "$x.pm" }
				@{$options->{modules}}
			] );
	}
	elsif ( $options->{requires} ) {
		$self->{init}->set_boot_use( $options->{requires} );
	}
	else {
		$self->{init}->set_boot_use( $self->default_boot_use );
	}

	$self->{init}->set_boot_lib(
		$options->{lib} || $self->default_boot_lib
	);
}

sub initialize {
	my $self = shift;
	$self->{init}->initialize;
}

sub parallel_for {
	my $self = shift;
	my $range = shift;
	my $body = shift;
	my $allocator = shift and croak 'no allocator allowed yet';
	# ... perhaps this should just be the public API.
	$body->parallel_for($range);
}

sub threads::tbb::init::CLONE_SKIP { 1 }

BEGIN {
	no strict 'refs';
	for my $body ( qw( for_int_array_func for_int_method ) ) {
		my $class = "threads::tbb::$body";
		*$body = sub {
			my $tbb = shift;
                        my $bfunc = eval { $class->new( $tbb->{init}, @_ ) };
			if ( !$bfunc ) {
				carp $@;
			}
			return $bfunc;
		};
		# body functions are not refcounted, don't clone them.
		*{"${class}::CLONE_SKIP"} = sub{1};
	}
}

1;
