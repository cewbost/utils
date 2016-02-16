# utils
Just some C++ utilities all header only.

These haven't been tested particularly thoroughly. If you encouter any bugs or even compile-time errors(!) please message me.

documentation can be generated with doxygen.

### bin_serializer
A class for serializing objects into a binary buffer. it simply memcpys objects of any type into the buffer

### cfheap
A cache-friendly heap structure that stores all elements into a continuous buffer. Don't really know why I made this since the C++ standard library implements priority\_queue the same way, but it preforms slightly better than glibc++ in my benchmarks.

###delaunay
A class for computing delaynay triangulations in O(n) = n \* log2(n), as well as the dual-graph (voronoi diagram). The output is suitable for rendering with opengl.

###intrusive_list
Intrusive linked list.

###pool_allocator
A simple allocator that allocates objects in fixed size chunks into a continuous buffer. Gives quick allocation and deallocation times and good locality of reference.

###ref_counted
Reference counting. Differs from std::shared\_ptr in that it requires that the reference count is a member of the object itself (this makes it possible to cast an Object\* to a counted\_ptr<Object>), and the reference count is not atomic, making it faster but not thread-safe.

###ring_buffer
A ring buffer class.

###worley
A class for generating worley noise (cell noise) in O(n) = n time. Can also be used for generating signed distance field maps. Supports calculating distance to any number of nearest points in the graph.

###sso_vector
A vector with "small size optimization". Elements are stored in the object itself avoiding dynamic allocation for arrays below a specified length.
