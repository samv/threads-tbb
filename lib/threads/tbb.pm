package threads::tbb;

use 5.008;
use strict;
use warnings;

our $VERSION = '0.01';

require XSLoader;
XSLoader::load('threads::tbb', $VERSION);

1;
