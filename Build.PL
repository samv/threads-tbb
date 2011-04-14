use 5.008;
use strict;
use warnings;
use Module::Build;
use Config;

if (not $Config{useithreads} or $Config{use5005threads} ) {
	print STDERR "Can't use threads::lite without threading support";
	exit 0;
}

my $builder = Module::Build->new(
	module_name         => 'threads::tbb',
	license             => 'perl',
	dist_author         => 'Sam Vilain <samv@cpan.org>',
	dist_version_from   => 'lib/threads/tbb.pm',
	configure_requires  => {
		'Module::Build' => 0,
	},
	build_requires => {
		'ExtUtils::CBuilder' => 0,
		'Module::Build' => 0,
		'Test::More' => 0,
	},
	requires            => {
		perl            => 5.008,
		Exporter        => 5.57,
	},
	c_source            => 'src',
	extra_compiler_flags => ['-x', 'c++'],
	add_to_cleanup      => [ 'threads-tbb-*' ],
	create_makefile_pl  => 'small',
	meta_merge => {
		resources => {
			bugtracker  => 'http://rt.cpan.org/NoAuth/Bugs.html?Dist=threads-tbb',
			repository  => 'http://github.com/openparallel/threads-tbb',
		},
		keywords       => [ 'threads', 'tbb' ],
	},
	sign                => 1,
);

$builder->create_build_script();
