
package threads::tbb::concurrent::array;

=head1 NAME

threads::tbb::concurrent::array - shared array variable via tbb::concurrent_vector

=head1 SYNOPSIS

  use threads::tbb;

  #my @array :concurrent;  # TODO
  tie my @array, "threads::tbb::concurrent::array";

=head1 DESCRIPTION

The concurrent vector is an array that multiple threads can read to
and write from.  These operations are thread-safe:

These are:

=item FETCH & STORE

This is storing or retrieving a set index.

L<threads::tbb::blocked_int> 

=item EXTEND

(TODO) could be implemented using grow_by and then calls to PUSH use
the grow_by.

=head2 THREAD UNSAFE OPERATIONS

These functions return information which can get out of date.  None of
them are safe, because you'd need to hold an exclusive lock on the
array to safely use them.

=over

=item FETCHSIZE

Get the length of the array.

=back

=cut

