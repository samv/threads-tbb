use 5.008;
use ExtUtils::MakeMaker;
# See lib/ExtUtils/MakeMaker.pm for details of how to influence
# the contents of the Makefile that is written.

my $CC = 'g++';

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
    INC               => '-I.', # e.g., '-I. -I/usr/include/other'
    #OBJECT            => '$(O_FILES)', # link all the C files too
    XSOPT             => '-C++ -hiertype',
    TYPEMAPS          => ['perlobject.map'],
    CC                => $CC,
    LD                => '$(CC)',
);

