#### TODOLIST

- Set up the dictionary system. Decide whether to use UMAP or MAP. Design the most suitable syntax. Also, prepare the pair structure.
- Add `foreach` support to `for` loops using the `in` keyword. Also, if the array elements are components, adding support like `for(x | y | z in transforms)` and auto-binding would be very helpful. The same applies to dictionaries; something like `for (key | value in dict)` would also work well.
- Add the `super` and `self` keywords to both `object` and `component`. I assume `super` won't make sense for a component.
