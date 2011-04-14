use 5.008;
use strict;
use warnings;
use ExtUtils::MakeMaker;
use Config;
use FindBin qw($Bin);
use File::Find;

if (not $Config{useithreads} or $Config{use5005threads} ) {
	print STDERR "Can't use threads::lite without threading support";
	exit 0;
}

my $CC = 'g++';

my (@clean, @OBJECT, @LD_FROM, %XS);
{
    my @xs;
    find(sub { m{\.xs$} && push @xs, $File::Find::name }, "lib");
    for my $xs (@xs) {
        (my $c = $xs) =~ s/\.xs$/.c/i;
        (my $o = $xs) =~ s/\.xs$/\$(OBJ_EXT)/i;
	#$o =~ s{.*/}{};

        $XS{$xs} = $c;
        push @OBJECT, $o;
	#$o =~ s{.*/}{};
	push @LD_FROM, $o;
        push @clean, $o;
    }

    for my $c (<src/*.cc>) {
        (my $o = $c) =~ s/\.cc$/\$(OBJ_EXT)/i;
        push @OBJECT, $o;
	#$o =~ s{.*/}{};
	push @LD_FROM, $o;
        push @clean, $o;
    }

    print STDERR "XS: @{[ %XS ]}\n";
    print STDERR "Object: @OBJECT\n";
    print STDERR "LDFrom: @LD_FROM\n";
}
WriteMakefile(
    NAME              => 'threads::tbb',
    VERSION_FROM      => 'lib/threads/tbb.pm', # finds $VERSION
    PREREQ_PM         => {
      'ExtUtils::XSpp' => '0.01',
    }, # e.g., Module::Name => 1.1
    ABSTRACT_FROM  => 'lib/threads/tbb.pod',
    AUTHOR         => 'Sam Vilain <sam.vilain@openparallel.com>',
    LIBS              => ['-ltbb'], # e.g., '-lm'
    DEFINE            => '', # e.g., '-DHAVE_SOMETHING'
    INC               => '-Isrc', # e.g., '-I. -I/usr/include/other'
    XSOPT             => '-C++ -hiertype',
    TYPEMAPS          => ["$Bin/src/typemap.xsc"],
    CC                => $CC,
    LD                => '$(CC)',
    CCFLAGS           => '-o $@',
    LDFROM => join (q{ }, @LD_FROM),
        clean  => { FILES => join(q{ }, @clean) },
        OBJECT => join (q{ }, @OBJECT),
        XS     => \%XS,
    depend => { '$(OBJECT)' => join(" ", glob "src/*.h") },
);

